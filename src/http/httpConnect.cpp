#include "http/httpConnect.h"
#include <cstring>

const char* HttpConnect::m_srcDir;

HttpConnect::HttpConnect(MySQLConnectionPool* mysql, RedisConnectionPool* redis)
{
    assert(mysql != nullptr);
    assert(redis != nullptr);

    m_fd = -1;
    m_isClosed = false;
    m_request = new HttpRequest(mysql, redis);
    m_response = new HttpResponse();
}

void HttpConnect::init(int fd, const sockaddr_in &addr)
{
    m_fd = fd;
    m_addr = addr;
    m_isClosed = false;
}

ssize_t HttpConnect::read(int *Errno)
{
    ssize_t readBytes = -1;
    while (true) {
        readBytes = m_readBuffer.readFd(m_fd, Errno);
        if (readBytes <= 0) {
            break;
        }
    }
    return readBytes;
}

ssize_t HttpConnect::write(int *Errno)
{
    ssize_t len = -1;

    while (true) {
        len = writev(m_fd, m_iov, m_iovCnt);
        if (len <= 0) {
            *Errno = errno;
            break;
        }
        
        if(m_iov[0].iov_len + m_iov[1].iov_len  == 0) {
            break;
        } else if(static_cast<size_t>(len) > m_iov[0].iov_len) {
            m_iov[1].iov_base = (uint8_t*) m_iov[1].iov_base + (len - m_iov[0].iov_len);
            m_iov[1].iov_len -= (len - m_iov[0].iov_len);
            if(m_iov[0].iov_len) {
                m_writeBuffer.retrieveAll();
                m_iov[0].iov_len = 0;
            }
        } else {
            m_iov[0].iov_base = (uint8_t*)m_iov[0].iov_base + len; 
            m_iov[0].iov_len -= len; 
            m_writeBuffer.retrieve(len);
        }
    }

    return len;
}

bool HttpConnect::process()
{
    m_request->Init();
    if(m_readBuffer.readAbleBytes() <= 0) {
        return false;
    }
    else if(m_request->parse(m_readBuffer)) {
        LOG_DEBUG("Request content is %s", m_request->path().c_str());
        m_response->Init(m_srcDir, m_request->path(), m_request->IsKeepAlive(), 200);
    } else {
        m_response->Init(m_srcDir, m_request->path(), false, 400);
    }
    
    m_response->MakeResponse(m_writeBuffer);

    m_iov[0].iov_base = const_cast<char*>(m_writeBuffer.readAddress());
    m_iov[0].iov_len = m_writeBuffer.readAbleBytes();
    m_iovCnt = 1;

    // 文件
    if(m_response->FileLen() > 0  && m_response->File()) {
        m_iov[1].iov_base = m_response->File();
        m_iov[1].iov_len = m_response->FileLen();
        m_iovCnt = 2;
    }
    LOG_DEBUG("filesize:%d, %d to %d", m_response->FileLen() , m_iovCnt, toWriteBytes());
    return true;
}
void HttpConnect::clearResource()
{
    m_fd = -1;
    m_request->Init();
}

void HttpConnect::closeClient()
{
    m_response->UnmapFile();
    m_isClosed = true;
    close(m_fd);
    LOG_INFO("Client [%d] quit", m_fd);
}