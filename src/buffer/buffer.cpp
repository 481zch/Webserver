#include "buffer/buffer.h"
#include <assert.h>
#include <sys/uio.h>

CircleBuffer::CircleBuffer(int initBufferSize, std::string end)
    : m_buffer(initBufferSize), m_end(std::move(end)), m_readPos(0), m_writePos(0) {}

// 读出所有数据
ssize_t CircleBuffer::readOut(int fd)
{
    if (m_readPos <= m_writePos) {
        ssize_t len = write(fd, readAddress(), readableBytes());
        m_readPos = m_writePos;
        return len;
    } else {
        ssize_t len_1 = write(fd, readAddress(), m_buffer.size() - 1 - m_readPos);
        ssize_t len_2 = write(fd, &m_buffer[0], m_writePos);
        m_readPos = m_writePos;
        return len_1 + len_2;
    }
}

ssize_t CircleBuffer::writeIn(int fd, int* Errno)
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
        *Errno = EAGAIN;
        return -1;
    }

    // 更新读写指针和Append操作
    if (static_cast<size_t>(len) <= iov[0].iov_len) {
        m_writePos += len;
    } else if (static_cast<size_t>(len) <= iov[0].iov_len + iov[1].iov_len) {
        m_writePos = iov[0].iov_len + iov[1].iov_len - len;
    } else {
        if (m_buffer.size() - 1 >= len) *Errno = EAGAIN;
        else *Errno = ENOMEM;
        return -1;
    }

    *Errno = 0;
    return len;
}

void CircleBuffer::Append(const std::string &str, int* Errno)
{
    Append(str.c_str(), str.size(), Errno);
}

void CircleBuffer::Append(const void *data, size_t len, int* Errno)
{
    Append(static_cast<const char*>(data), len, Errno);
}

void CircleBuffer::Append(const CircleBuffer &buff, int* Errno)
{
    Append(buff.readAddress(), buff.readableBytes(), Errno);
}

void CircleBuffer::Append(const char *str, size_t len, int* Errno)
{
    assert(str);
    ensureWriteable(len, Errno);
    if (*Errno != 0) return;
    writeToBuffer(str, len);
}

std::string CircleBuffer::getReadableBytes()
{
    // 修正
    if (m_readPos == m_buffer.size() - 1) m_readPos = 0;

    if (m_readPos <= m_writePos)
    {
        std::string res(m_buffer.begin() + m_readPos, m_buffer.begin() + m_writePos);
        m_readPos = m_writePos;
        return res;
    }
    else
    {
        std::string s1(m_buffer.begin() + m_readPos, m_buffer.end() - 1);
        std::string s2(m_buffer.begin(), m_buffer.begin() + m_writePos);
        m_readPos = m_writePos;
        return s1 + s2;
    }
}

std::string CircleBuffer::getByEndBytes(int* Errno)
{
    std::string res;
    size_t endPos = 0;

    if (m_readPos < m_writePos) {
        std::string data = std::string(m_buffer.begin() + m_readPos, m_buffer.begin() + m_writePos);
        if ((endPos = data.find(m_end)) != std::string::npos) {
            // 找到结束符，提取数据并更新读指针
            res = data.substr(0, endPos + m_end.length());
            m_readPos += res.size();
            *Errno = 0;
            return res;
        }
    } else if (m_readPos > m_writePos) {
        // 右区间，两边，左区间
        std::string data_right = std::string(m_buffer.begin() + m_readPos, m_buffer.end() - 1);
        std::string data_left = std::string(m_buffer.begin(), m_buffer.begin() + m_writePos);
        std::string data = data_right + data_left;

        if ((endPos = data.find(m_end)) != std::string::npos) {
            if (endPos + m_end.size() <= data_right.size()) {
                res = data_right.substr(0, endPos + m_end.size());
                m_readPos += res.size();
            } else if (endPos >= data_left.size()) {
                size_t part_in_left = endPos - data_right.size();
                res = data_right + data_left.substr(0, part_in_left + m_end.size());
                m_readPos = part_in_left + m_end.size();
            } else {
                int remaing_left = m_end.size() - (data_right.size() - endPos);
                res = data_right + data_left.substr(0, remaing_left);
                m_readPos = remaing_left;
            }
            *Errno = 0;
            return res;
        }
    }

    *Errno = EAGAIN;
    return res;
}

size_t CircleBuffer::readableBytes() const
{
    if (m_readPos <= m_writePos) return m_writePos - m_readPos;
    else {
        return m_buffer.size() - 1 - m_readPos + m_writePos;
    }
}

size_t CircleBuffer::writeableBytes() const
{
    return m_buffer.size() - 1 - readableBytes();
}

void CircleBuffer::writeToBuffer(const char *str, size_t len)
{
    // 修正写位置
    if (m_writePos == m_buffer.size() - 1) m_writePos = 0;

    if (m_writePos < m_readPos)
    {
        std::copy(str, str + len, m_buffer.begin() + m_writePos);
        m_writePos += len;
        return;
    }
    else
    {
        int distance = m_buffer.size() - m_writePos - 1;
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

const char *CircleBuffer::readAddress() const
{
    return &m_buffer[m_readPos];
}

void CircleBuffer::ensureWriteable(size_t len, int* Errno)
{
    size_t space = writeableBytes();
    // 立即写，不能写，等待写
    if (space >= len) {
        *Errno = 0;
        return;
    } else if (len > m_buffer.size() - 1) {
        *Errno = ENOMEM;
        return;
    } else {
        *Errno = EAGAIN;
        return;
    }
}