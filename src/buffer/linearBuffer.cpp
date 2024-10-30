#include "linearBuffer.h"
#include <algorithm>

LinearBuffer::LinearBuffer(size_t capacity): m_readPos(0), m_writePos(0), m_buffer(capacity)
{
}

ssize_t LinearBuffer::readFd(int fd, int *Errno)
{
    char tempBuff[65535];
    struct iovec iov[2];
    size_t writable = remainCapacity();

    // 配置 iovec 结构，用于分散读
    iov[0].iov_base = &m_buffer[m_writePos];
    iov[0].iov_len = writable;
    iov[1].iov_base = tempBuff;
    iov[1].iov_len = sizeof(tempBuff);

    ssize_t len = readv(fd, iov, 2);
    if (len < 0) {
        *Errno = errno;
    } else if (static_cast<size_t>(len) <= writable) {
        m_writePos += len;
    } else {
        m_writePos = m_buffer.size();
        append(tempBuff, static_cast<size_t>(len - writable));
    }

    return len;
}

ssize_t LinearBuffer::writeFd(int fd, int *Errno)
{
    ssize_t len = write(fd, &m_buffer[m_readPos], readAbleBytes());
    if (len < 0) {
        *Errno = errno;
        return len;
    }
    m_readPos += len;
    return len;
}

void LinearBuffer::append(const std::string &str)
{
    append(str.c_str(), str.size());
}

void LinearBuffer::append(const char *str, size_t len)
{
    size_t curSize = remainCapacity();
    if (curSize < len) {
        size_t newCapacity = len - curSize + m_buffer.size();
        expandSpace(newCapacity);
    }
    writeToBuffer(str, len);
}

std::string LinearBuffer::getByEndFlag(const std::string &endFlag)
{
    auto it = std::search(m_buffer.begin() + m_readPos, m_buffer.begin() + m_writePos,
                          endFlag.begin(), endFlag.end());
    if (it == m_buffer.begin() + m_writePos) {
        return "";
    } else {
        std::string result(m_buffer.begin() + m_readPos, it);
        m_readPos = std::distance(m_buffer.begin(), it) + endFlag.size();
        return result;
    }
}

size_t LinearBuffer::readAbleBytes() const
{
    return m_writePos - m_readPos;
}

std::string LinearBuffer::getReadAbleBytes()
{
    std::string result(m_buffer.begin() + m_readPos, m_buffer.begin() + m_writePos);
    m_readPos = m_writePos = 0;
    return result;
}

std::string LinearBuffer::getDataByLength(size_t len)
{
    std::string result;
    size_t existBytes = readAbleBytes();
    if (len <= existBytes) {
        result.assign(m_buffer.begin() + m_readPos, m_buffer.begin() + m_readPos + len);
        m_readPos += len;
    }
    return result;
}

std::string LinearBuffer::justGetData()
{
    std::string result(m_buffer.begin() + m_readPos, m_buffer.begin() + m_writePos);
    return result;
}

void LinearBuffer::writeToBuffer(const char *str, size_t len)
{
    size_t tailSpace = m_buffer.end() - m_buffer.begin() - m_writePos;
    if (tailSpace < len) {
        moveTailToHead();
    }
    memcpy(m_buffer.data() + m_writePos, str, len);
    m_writePos += len;
}

void LinearBuffer::expandSpace(size_t len)
{
    // 移动数据，扩大空间
    moveTailToHead();
    m_buffer.resize(len);
}

size_t LinearBuffer::remainCapacity() const
{
    return m_buffer.size() - m_writePos;
}

void LinearBuffer::moveTailToHead()
{
    size_t lastUsage = m_writePos - m_readPos;
    std::copy(m_buffer.begin() + m_readPos, m_buffer.begin() + m_writePos, m_buffer.begin());
    m_readPos = 0, m_writePos = lastUsage;
}
