#include "http/httpConnect.h"

HttpConnect::HttpConnect(std::shared_ptr<MySQLConnectionPool> mysql, std::shared_ptr<RedisConnectionPool> redis)
{
    m_fd = -1;
    m_ssl = nullptr;
    m_request = std::make_shared<HttpRequest>(mysql, redis);
    m_response = std::make_shared<HttpResponse>();
}

void HttpConnect::init(int fd, const sockaddr_in &addr, SSL *ssl)
{
    m_fd = fd;
    m_addr = addr;
    m_ssl = ssl;
}

ssize_t HttpConnect::read(int *Errno)
{
    // 这里写的也是过于粗暴了，为了使用环形缓冲区而去使用
    char str[4096];
    ssize_t readBytes = 0;
    int ret = 0;

    while (true) {
        ret = SSL_read(m_ssl, str, sizeof(str));
        if (ret > 0) {
            m_readBuffer.Append(str);
            readBytes += ret;
        } else {
            *Errno = errno;
            break;
        }
    }
    return readBytes;
}

ssize_t HttpConnect::write(int *Errno)
{
    ssize_t writeBytes = 0;
    int ret = 0;

    while (true) {
        if (m_iov[0].iov_len > 0) {
            ret = SSL_write(m_ssl, m_iov[0].iov_base, m_iov[0].iov_len);
            if (ret > 0) {
                writeBytes += ret;
                m_iov[0].iov_base = static_cast<char*>(m_iov[0].iov_base) + ret;
                m_iov[0].iov_len -= ret;
            } else {
                *Errno = errno;
                break;
            }
        }

        if (m_iov[0].iov_len == 0 && m_iov[1].iov_len > 0) {
            ret = SSL_write(m_ssl, m_iov[1].iov_base, m_iov[1].iov_len);
            if (ret > 0) {
                writeBytes += ret;
                m_iov[1].iov_base = static_cast<char*>(m_iov[1].iov_base) + ret;
                m_iov[1].iov_len -= ret;
            } else {
                *Errno = errno;
                break;
            }
        }

        if (m_iov[0].iov_len == 0 && m_iov[1].iov_len == 0) {
            break;
        }
    }

    return writeBytes;
}

bool HttpConnect::process()
{
    m_request->Init();
    if(m_readBuffer.readableBytes() <= 0) {
        return false;
    }
    else if(m_request->parse(m_readBuffer)) {
        LOG_DEBUG("%s", m_request->path().c_str());
        m_response->Init(m_srcDir, m_request->path(), m_request->IsKeepAlive(), 200);
    } else {
        m_response->Init(m_srcDir, m_request->path(), false, 400);
    }

    m_response->MakeResponse(m_writeBuffer);
    
    // 响应头
    std::string tempData = m_writeBuffer.getReadableBytes();
    m_tempBuff.assign(tempData.begin(), tempData.end());

    m_iov[0].iov_base = static_cast<void*>(m_tempBuff.data());;
    m_iov[0].iov_len = m_tempBuff.size();
    m_iovCnt = 1;

    // 文件
    if(m_response->FileLen() > 0  && m_response->File()) {
        m_iov[1].iov_base = m_response->File();
        m_iov[1].iov_len = m_response->FileLen();
        m_iovCnt = 2;
    }
    LOG_DEBUG("filesize:%d, %d  to %d", m_response->FileLen() , m_iovCnt, toWriteBytes());
    return true;
}