#pragma once
#include <iostream>
#include <vector>
#include <atomic>
#include <unistd.h>

/*
缓冲区不是线程安全的,需要使用的部分来确认缓冲区的线程安全
使用环形缓冲区避免头部存在数据发生拷贝移动
可以完成自动覆盖，使得空间更加高效的利用
读取范围为左闭右开型
*/

class CircleBuffer {
public:
    CircleBuffer(int initBufferSize = 1024);
    ssize_t readOut(int fd);
    ssize_t writeIn(int fd);
    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const void* data, size_t len);
    void Append(const CircleBuffer& buff);
    std::string getReadableBytes();

private:
    std::vector<char> m_buffer;
    std::atomic<std::size_t> m_readPos;
    std::atomic<std::size_t> m_writePos;

    size_t readableBytes() const;
    size_t writeableBytes() const;
    void writeToBuffer(const char* str, size_t len);
    void reset();

    const char* peek() const;

    void ensureWriteable(size_t len);
    void makeSpace(size_t len);
};