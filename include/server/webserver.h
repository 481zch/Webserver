#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unordered_map>

#include "pool/connectPool.h"
#include "pool/objectPool.h"
#include "pool/threadPool.h"
#include "http/httpsConnect.h"
#include "log/log.h"
#include "ssl/ssl.h"

/*
 除了支持reactor模式外，还支持proactor模式
 对于proactor模式，使用io_using来完成
 内核完成的是把http请求读取到缓冲区，
 然后回调函数给线程池的提交，
 接着再由线程池完成请求报文的解析和组装，
 最后由内核写入到缓冲区，再完成发送。
 */

class webserver {
public:

private:
    bool m_useReactor;
    bool m_stop;

    std::unique_ptr<ThreadPool> m_threadPool;
    std::unique_ptr<MySQLConnectionPool> m_sqlConnectPool;
    std::unique_ptr<RedisConnectionPool> m_redisConnectPool;
    std::unique_ptr<ObjectPool<HttpsConnect>> m_objectPool;

    int port;
    char* m_srcDir;

    std::unordered_map<int, HttpsConnect*> mp;

};



