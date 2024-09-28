#include "buffer/buffer.h"
#include <assert.h>
#include <sys/uio.h>

CircleBuffer::CircleBuffer(int initBufferSize): m_buffer(initBufferSize), m_readPos(0), m_writePos(0) {}

ssize_t CircleBuffer::readOut(int fd)
{
    size_t readable = readableBytes();
    if (readable == 0) {
        return 0;
    }

    if (m_readPos <= m_writePos)
    {
        ssize_t len = write(fd, peek(), readableBytes());
        m_writePos.store(m_readPos.load());
        return len;
    }
    else
    {
        ssize_t len_1 = write(fd, peek(), m_buffer.size() - m_readPos);
        ssize_t len_2 = write(fd, &m_buffer[0], m_writePos);
        m_writePos.store(m_readPos.load());
        return len_1 + len_2;
    }
}

ssize_t CircleBuffer::writeIn(int fd)
{
    char buff[65536];
    struct iovec iov[3];

    if (m_writePos < m_readPos)
    {
        iov[0].iov_base = &m_buffer[m_writePos];
        iov[0].iov_len = m_readPos - m_writePos;
        iov[1].iov_base = nullptr;
        iov[1].iov_len = 0;
    }
    else
    {
        iov[0].iov_base = &m_buffer[m_writePos];
        iov[0].iov_len = m_buffer.size() - m_writePos;
        iov[1].iov_base = &m_buffer[0];
        iov[1].iov_len = m_readPos;
    }

    iov[2].iov_base = buff;
    iov[2].iov_len = sizeof(buff);

    ssize_t len = readv(fd, iov, 3);
    if (len < 0) {
        return -1;
    }

    // 更新读写指针和Append操作
    if (static_cast<size_t>(len) <= iov[0].iov_len) {
        m_writePos = m_writePos + len;
    } else if (static_cast<size_t>(len) <= iov[0].iov_len + iov[1].iov_len) {
        m_writePos = iov[0].iov_len + iov[1].iov_len - len;
    } else {
        size_t writtenInBuffer = iov[0].iov_len + iov[1].iov_len;
        m_writePos.store(m_readPos.load());
        Append(buff, static_cast<size_t>(len - writtenInBuffer));
    }

    return len;
}

void CircleBuffer::Append(const std::string &str)
{
    Append(str.c_str(), str.size());
}

void CircleBuffer::Append(const char *str, size_t len)
{
    assert(str);
    ensureWriteable(len);
    writeToBuffer(str,len);
}

void CircleBuffer::Append(const void *data, size_t len)
{
    Append(static_cast<const char*>(data), len);
}

void CircleBuffer::Append(const CircleBuffer &buff)
{
    Append(buff.peek(), buff.readableBytes());
}

std::string CircleBuffer::getReadableBytes()
{
    if (m_readPos <= m_writePos)
    {
        std::string res(m_buffer.begin() + m_readPos, m_buffer.begin() + m_writePos);
        m_writePos.store(m_readPos.load());
        return res;
    }
    else
    {
        std::string s1(m_buffer.begin() + m_readPos, m_buffer.end());
        std::string s2(m_buffer.begin(), m_buffer.begin() + m_writePos);
        m_writePos.store(m_readPos.load());
        return s1 + s2;
    }
}

size_t CircleBuffer::readableBytes() const
{
    if (m_readPos <= m_writePos) return m_writePos - m_readPos;
    else {
        return m_buffer.size() - m_readPos + m_writePos;
    }
}

size_t CircleBuffer::writeableBytes() const
{
    if (m_writePos >= m_readPos) return m_buffer.size() - m_writePos + m_readPos;
    else return m_readPos - m_writePos;
}

void CircleBuffer::writeToBuffer(const char *str, size_t len)
{
    if (m_writePos <= m_readPos)
    {
        std::copy(str, str + len, m_buffer.begin() + m_writePos);
        m_writePos += len;
        return;
    }
    else
    {
        int distance = m_buffer.size() - m_writePos;
        if (distance >= len) {
            std::copy(str, str + len, m_buffer.begin() + m_writePos);
            m_writePos += len;
            return;
        } else {
            std::copy(str, str + distance, m_buffer.begin() + m_writePos);
            std::copy(str + distance, str + len, m_buffer.begin());
            m_writePos = len - distance;
            return;
        }
    }
}

void CircleBuffer::reset()
{
    m_buffer.clear();
    m_readPos = 0;
    m_writePos = 0;
}

const char *CircleBuffer::peek() const
{
    return &m_buffer[m_readPos];
}

void CircleBuffer::ensureWriteable(size_t len)
{
    size_t space = writeableBytes();
    if (space < len) {
        makeSpace(len - space);
    }
}

// 传入参数表示的是需要拓展的增长量
void CircleBuffer::makeSpace(size_t len)
{
    if (m_writePos >= m_readPos) {
        m_buffer.resize(m_buffer.size() + len);
    }
    else {
        size_t distance = m_writePos;
        size_t last = m_buffer.size();
        len = std::max(distance, len);
        m_buffer.resize(len + m_buffer.size());
        // 移动前部数据
        std::copy(m_buffer.begin(), m_buffer.begin() + distance, m_buffer.begin() + last);
        m_writePos = m_readPos + distance;
    }
}