#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <unordered_map>
#include <iostream>

#include "pool/objectPool.h"
#include "pool/threadPool.h"
#include "http/httpConnect.h"
#include "timer/heapTimer.h"
#include "log/log.h"
#include "epoller.h"

class Webserver {
public:
    Webserver(int threadNum = 10, int connectNum = 10, size_t objectNum = 10, 
              int port = 1317, int sqlPort = 3306, int redisPort = 6379, const char* host = "192.168.19.133",
              const char* dbName = "webserverDB", const char* sqlUser = "root", const char* sqlPwd = "123456",
              int timeoutMS = 60000, int MAX_FD = 65535, size_t userCount = 0);
    ~Webserver();
    void eventLoop();
    static void setCloseServer(int) {m_stop = true;}

private:
    ThreadPool* m_threadPool;
    MySQLConnectionPool* m_sqlConnectPool;
    RedisConnectionPool* m_redisConnectPool;
    ObjectPool<HttpConnect>* m_objectPool;

    HeapTimer* m_timer;
    Epoller* m_epoller;

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
    void initEventMode();
    void extentTime(HttpConnect* client);

    void dealListen();
    void closeConn(const std::string& message, HttpConnect* client);
    void dealRead(HttpConnect* client);
    void dealWrite(HttpConnect* client);
    void onProcess(HttpConnect* client);
    void sendError(int fd, const char* info);

    void addClient(int fd, sockaddr_in addr);
    void onRead(HttpConnect* client);
    void onWrite(HttpConnect* client);
};