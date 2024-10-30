#include "server/webserver.h"

std::atomic<bool> Webserver::m_stop = false;

Webserver::Webserver(int threadNum, int connectNum, size_t objectNum, 
                    int port, int sqlPort, int redisPort, const char* host,
                    const char *dbName, const char *sqlUser, const char *sqlPwd,
                    int timeoutMS, int MAX_FD, size_t userCount):
                    m_threadPool(new ThreadPool(threadNum)), m_sqlConnectPool(new MySQLConnectionPool(host, sqlUser, sqlPwd, dbName, sqlPort)),
                    m_redisConnectPool(new RedisConnectionPool(host, redisPort)), m_epoller(new Epoller), m_port(port),
                    m_timer(new HeapTimer), m_timeoutMS(timeoutMS), MAX_FD(MAX_FD), m_userCount(userCount)
{
    LOG_INFO("========== Server init ==========");
    initEventMode();

    m_srcDir = "/project/webserver/resources";
    HttpConnect::m_srcDir = m_srcDir;

    m_objectPool = new ObjectPool<HttpConnect>(objectNum, m_sqlConnectPool, m_redisConnectPool);
    if (!initSocket()) {
        m_stop = true;
        LOG_ERROR("Socket init error!");
    }
}

Webserver::~Webserver()
{
    close(m_listenFd);
    m_stop = true;
    m_threadPool->shutdown();

    // 释放资源
    delete m_threadPool;
    delete m_sqlConnectPool;
    delete m_redisConnectPool;
    delete m_objectPool;
    delete m_epoller;
    delete m_timer;
}

void Webserver::eventLoop()
{
    int timeMS = -1;
    if(!m_stop) { LOG_INFO("========== Server start =========="); }
    while (!m_stop) {
        timeMS = m_timer->GetNextTick();    // 默认返回的是-1
        int eventCount = m_epoller->wait();
        for (int i = 0; i < eventCount; ++ i) {
            int fd = m_epoller->getEventFd(i);
            uint32_t events = m_epoller->getEvents(i);
            if (fd == m_listenFd) {
                dealListen();
            } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(mp_users.count(fd));
                closeConn(std::string("Epoll cause close."), mp_users[fd]);
            } else if (events & EPOLLIN) {
                assert(mp_users.count(fd) > 0);
                dealRead(mp_users[fd]);
            } else if (events & EPOLLOUT) {
                assert(mp_users.count(fd) > 0);
                dealWrite(mp_users[fd]);
            } else {
                LOG_ERROR("Unexpected event on fd[%d]: events = 0x%x", fd, events);
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
            addClient(fd, addr);
        }
    }
}

void Webserver::closeConn(const std::string& message, HttpConnect *client)
{
    // 这行代码的原因是在时间堆回调的时候，这个对象已经被回收到池中了，从哈希表中已经移除，因此是无法找到的    
    if (client == nullptr) return;
    // 调试使用
    LOG_INFO("Client[%d] quit, the quit reason is: %s", client->getFd(), message.c_str());
    m_epoller->delFd(client->getFd());
    client->closeClient();
    client->m_isClosed = true;
    mp_users.erase(client->getFd());
    client->clearResource();
    m_objectPool->releaseObject(client);
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
        m_epoller->modFd(client->getFd(), m_connEvent | EPOLLOUT);
    } else {
        m_epoller->modFd(client->getFd(), m_connEvent | EPOLLIN);       
    }
}

void Webserver::addClient(int fd, sockaddr_in addr)
{
    assert(fd > 0);
    auto obj = m_objectPool->acquireObject();
    obj->init(fd, addr);
    if (m_timeoutMS > 0) {
        m_timer->add(fd, m_timeoutMS, std::bind(&Webserver::closeConn, this, std::string("Timer cause client close"), mp_users[fd]));
    }
    // 设置读事件到来的epoll触发
    m_epoller->addFd(fd, EPOLLIN | m_connEvent);
    setFdNonBlock(fd);
    mp_users[fd] = obj;
    ++ m_userCount;
    LOG_INFO("Client[%d] in!", fd);
}

void Webserver::sendError(int fd, const char *info)
{
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        LOG_WARN("Send error to client[%d] error!", fd);
    }
    close(fd);
}

bool Webserver::initSocket()
{
    int ret;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(m_port);

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
    ret = m_epoller->addFd(m_listenFd,  m_listenEvent | EPOLLIN);
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
    m_listenEvent = EPOLLRDHUP;
    m_connEvent = EPOLLONESHOT | EPOLLRDHUP; 

    m_connEvent |= EPOLLET;
    m_listenEvent |= EPOLLET;
}

void Webserver::extentTime(HttpConnect *client)
{
    assert(client);
    if (m_timeoutMS > 0) {
        m_timer->adjust(client->getFd(), m_timeoutMS);
    }
}

void Webserver::onRead(HttpConnect *client)
{
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    if (ret <= 0 && readErrno != EAGAIN) {
        closeConn(std::string("Read Error cause client close."), client);
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
    if (client->toWriteBytes() == 0) {
        if (client->isKeepAlive()) {
            m_epoller->modFd(client->getFd(), m_connEvent | EPOLLIN);
            return;
        }
    } else if (ret < 0) {
        // 继续传输
        if (writeErrno == EAGAIN) {
            m_epoller->modFd(client->getFd(), m_connEvent | EPOLLOUT);
            return;
        }
    }
    closeConn(std::string("Write error cause client close"), client);
}