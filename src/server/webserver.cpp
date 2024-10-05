#include "server/webserver.h"
#include <string.h>

Webserver::Webserver(int threadNum, int connectNum, int objectNum, 
                    int port, int sqlPort, int redisPort, const char* host,
                    const char *dbName, const char *sqlUser, const char *sqlPwd,
                    int timeoutMS, int MAX_FD, size_t userCount, const char* certFile, const char* keyFile):
                    m_threadPool(new ThreadPool(threadNum)), m_sqlConnectPool(new MySQLConnectionPool(host, sqlUser, dbName, sqlPort)),
                    m_redisConnectPool(new RedisConnectionPool(host, redisPort)), m_objectPool(new ObjectPool<HttpConnect>(objectNum)),
                    m_timer(new HeapTimer), m_SSL(new SSLServer(certFile, keyFile)), m_timeoutMS(timeoutMS), MAX_FD(MAX_FD), m_userCount(userCount)
{
    LOG_INFO("========== Server init ==========");
    // 资源目录初始化
    m_srcDir = getcwd(nullptr, 256);
    assert(m_srcDir);

    initEventMode();
    if (!initSocket()) {
        m_stop = true;
        LOG_ERROR("Socket init error!");
    }

    if (!m_SSL->init()) {
        m_stop = true;
        LOG_ERROR("SSL init error!");
    }
}

Webserver::~Webserver()
{
    // 构造函数中资源的释放
    close(m_listenFd);
    m_stop = true;
    m_threadPool->shutdown();
}

void Webserver::eventLoop()
{
    int timeMS = -1;
    if(!m_stop) { LOG_INFO("========== Server start =========="); }
    while (!m_stop) {
        timeMS = m_timer->getNextTick();
        int eventCount = m_epoller->Wait();
        for (int i = 0; i < eventCount; ++ i) {
            int fd = m_epoller->GetEventFd(i);
            uint32_t events = m_epoller->GetEvents(i);
            if (fd == m_listenFd) {
                dealListen();
            } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(mp_users.count(fd));
                closeConn(mp_users[fd]);
            } else if (events & EPOLLIN) {
                assert(mp_users.count(fd));
                dealRead(mp_users[fd]);
            } else if (events & EPOLLOUT) {
                assert(mp_users.count(fd));
                dealWrite(mp_users[fd]);
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void Webserver::dealListen()
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    while (true) {
        int fd = accept(m_listenFd, (struct sockaddr*)&addr, &len);
        if (fd <= 0) return;
        else if (m_userCount >= MAX_FD) {
            sendError(fd, "Server busy!");
            LOG_WARN("Client is full");
            close(fd);
            return;
        }
        else {
            // 进行SSL握手
            SSL* ssl = nullptr;
            if (m_SSL->SSLGetConnection(fd, ssl)) {
                addClient(fd, addr, ssl);
            } else {
                sendError(fd, "SSL shake hands fail!");
                LOG_WARN("SSL shake hands fail!");
                close(fd);
                return;
            }
            
        }
    }
}

void Webserver::closeConn(HttpConnect *client)
{
    assert(client);
    LOG_INFO("Client[%d] quit!", client->getFd());
    m_epoller->DelFd(client->getFd());
    mp_users.erase(client->getFd());
    m_objectPool->releaseObject(std::unique_ptr<HttpConnect>(client));
    -- m_userCount;
}

void Webserver::dealRead(HttpConnect *client)
{
    assert(client);
    extentTime(client);
    auto task = std::bind(&Webserver::onRead, this, client);
    m_threadPool->submit(task);
}

void Webserver::dealWrite(HttpConnect *client)
{
    assert(client);
    extentTime(client);
    auto task = std::bind(&Webserver::onWrite, this, client);
    m_threadPool->submit(task);
}

void Webserver::onProcess(HttpConnect *client)
{
    if (client->process()) {
        m_epoller->ModFd(client->getFd(), m_connEvent | EPOLLOUT);
    } else {
        m_epoller->ModFd(client->getFd(), m_connEvent | EPOLLIN);       
    }
}

void Webserver::addClient(int fd, sockaddr_in addr, SSL* ssl)
{
    assert(fd > 0);
    auto obj = m_objectPool->acquireObject();
    obj->init(fd, addr, ssl);
    if (m_timeoutMS > 0) {
        m_timer->addTimer(TimerNode());
    }
    m_epoller->AddFd(fd, EPOLLIN | m_connEvent);
    setFdNonBlock(fd);
    mp_users[fd] = obj.get();
    ++ m_userCount;
    LOG_INFO("Client[%d] in!", mp_users[fd]->getFd());
}

void Webserver::sendError(int fd, const char *info)
{
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        LOG_WARN("Send error to client[%d] error!", fd);
    }
    closeConn(mp_users[fd]);
}

bool Webserver::initSocket()
{
    int ret;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htonl(m_port);

    m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenFd < 0) {
        LOG_ERROR("Create socket error");
        return false;
    }

    int optval = 1;
    ret = setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if (ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(m_listenFd);
        return false;
    }

    ret = bind(m_listenFd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0) {
        LOG_ERROR("Bind Port:%d error!");
        close(m_listenFd);
        return false;
    }

    ret = listen(m_listenFd, 8);
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!");
        close(m_listenFd);
        return false;
    }

    // 可读事件，当有新的客户端连接时触发
    ret = m_epoller->AddFd(m_listenFd,  m_listenEvent | EPOLLIN);
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(m_listenFd);
        return false;
    }

    setFdNonBlock(m_listenFd); 
    LOG_INFO("Server port:%d", m_port);
    return true;
}

int Webserver::setFdNonBlock(int fd)
{
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

void Webserver::initEventMode()
{
    m_connEvent |= EPOLLET;
    m_listenEvent |= EPOLLET;
}

void Webserver::extentTime(HttpConnect *client)
{
    assert(client);
    if (m_timeoutMS > 0) {
        m_timer->adjustTimer(client->getFd(), m_timeoutMS);
    }
}

void Webserver::onRead(HttpConnect *client)
{
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret - client->read(&readErrno);
    if (ret <= 0 && readErrno != EAGAIN) {
        closeConn(client);
        return;
    }
    onProcess(client);
}

void Webserver::onWrite(HttpConnect *client)
{
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if (client->ToWriteBytes() == 0) {
        if (client->IsKeepAlive()) {
            m_epoller->ModFd(client->getFd(), m_connEvent | EPOLLIN);
            return;
        }
    } else if (ret < 0) {
        if (writeErrno == EAGAIN) {
            m_epoller->ModFd(client->getFd(), m_connEvent | EPOLLOUT);
            return;
        }
    }
    closeConn(client);
}