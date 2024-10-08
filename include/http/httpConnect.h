#pragma once
#include <netinet/in.h>
#include "log/log.h"
#include "ssl/ssl.h"
#include "buffer/buffer.h"
#include "http/httpRequest.h"
#include "http/httpResponse.h"
#include "pool/connectPool.h"

class HttpConnect {
public:
    HttpConnect(std::shared_ptr<MySQLConnectionPool> mysql, std::shared_ptr<RedisConnectionPool> redis);
    ~HttpConnect();

    int getFd() const {return m_fd;}
    SSL* getSSL() const {return m_ssl;}
    void init(int fd, const sockaddr_in& addr, SSL* ssl);

    ssize_t read(int* Errno);
    ssize_t write(int* Errno);
    bool process();

    bool isKeepAlive() const {return m_request->IsKeepAlive();}
    int toWriteBytes() {return m_iov[0].iov_len + m_iov[1].iov_len;}

    static const char* m_srcDir;

private:
    int m_fd;
    sockaddr_in m_addr;
    SSL* m_ssl;
    iovec m_iov[2];
    size_t m_iovCnt;

    std::vector<char> m_tempBuff; // 为了使用环形缓冲区，使用一个栈上缓冲区
    CircleBuffer m_readBuffer;
    CircleBuffer m_writeBuffer;

    std::shared_ptr<HttpRequest> m_request;
    std::shared_ptr<HttpResponse> m_response;
};