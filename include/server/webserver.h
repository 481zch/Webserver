#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unordered_map>
#include <iostream>

#include "pool/connectPool.h"
#include "pool/objectPool.h"
#include "pool/threadPool.h"
#include "http/httpConnect.h"
#include "timer/heapTimer.h"
#include "log/log.h"
#include "ssl/ssl.h"
#include "epoller.h"

class Webserver {
public:
    Webserver(int threadNum = 10, int connectNum = 10, int objectNum = 10, 
              int port = 1316, int sqlPort = 3306, int redisPort = 6379, const char* host = "192.168.19.133",
              const char* dbName = "webserverDB", const char* sqlUser = "root", const char* sqlPwd = "123456",
              int timeoutMS = 60000, int MAX_FD = 65535, size_t userCount = 0,
              const char* certFile = "../../sslCertFile/certFile.pem", const char* keyFile = "../../sslCertFile/keyFile.pem");
    ~Webserver();
    void eventLoop();
    static void setCloseServer(int) {m_stop = true;}

private:
    std::unique_ptr<ThreadPool> m_threadPool;
    std::shared_ptr<MySQLConnectionPool> m_sqlConnectPool;
    std::shared_ptr<RedisConnectionPool> m_redisConnectPool;
    std::unique_ptr<ObjectPool<HttpConnect>> m_objectPool;

    std::unique_ptr<HeapTimer> m_timer;
    std::unique_ptr<Epoller> m_epoller;
    std::unique_ptr<SSLServer> m_SSL;

    static std::atomic<bool> m_stop;
    int m_port;
    char* m_srcDir;
    int m_listenFd;
    int m_timeoutMS;
    const int MAX_FD;
    size_t m_userCount;
    std::unordered_map<int, HttpConnect*> mp_users;
    
    uint32_t m_listenEvent;
    uint32_t m_connEvent;

    bool initSocket();
    int setFdNonBlock(int fd);
    void initEventMode(); // 初始化客户端监听和连接的触发模式
    void extentTime(HttpConnect* client);
    void initRescourceDir();

    void dealListen();
    void closeConn(HttpConnect* client);
    void dealRead(HttpConnect* client);
    void dealWrite(HttpConnect* client);
    void onProcess(HttpConnect* client);   // 业务逻辑的处理
    void sendError(int fd, const char* info);

    void addClient(int fd, sockaddr_in addr, SSL* ssl);
    void onRead(HttpConnect* client);
    void onWrite(HttpConnect* client);
};