#pragma once
#include <netinet/in.h>
#include "log/log.h"
#include "buffer/linearBuffer.h"
#include "http/httpRequest.h"
#include "http/httpResponse.h"

class HttpConnect {
public:
    HttpConnect(MySQLConnectionPool* mysql, RedisConnectionPool* redis);
    ~HttpConnect() = default;

    int getFd() const {return m_fd;}
    void init(int fd, const sockaddr_in& addr);

    ssize_t read(int* Errno);
    ssize_t write(int* Errno);
    bool process();

    bool isKeepAlive() const {return m_request->IsKeepAlive();}
    int toWriteBytes() {return m_iov[0].iov_len + m_iov[1].iov_len;}

    void clearResource();
    void closeClient();

    static const char* m_srcDir;
    bool m_isClosed;

private:
    int m_fd;
    sockaddr_in m_addr;
    iovec m_iov[2];
    size_t m_iovCnt;

    std::vector<char> m_tempBuff;
    LinearBuffer m_readBuffer;
    LinearBuffer m_writeBuffer;

    HttpRequest* m_request;
    HttpResponse* m_response;
};