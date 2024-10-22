#pragma once
#include <iostream>
#include <vector>
#include <unistd.h>

class CircleBuffer {
public:
    // 有一个额外空间是标示位置
    CircleBuffer(int initBufferSize = 10000 + 1, std::string end = "\r\n");
    ssize_t readOut(int fd);
    ssize_t writeIn(int fd);
    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const CircleBuffer& buff);
    void Append(const void* data, size_t len);
    std::string getReadableBytes();
    std::string getByEndBytes();
    size_t getCurBufferSize() const {return m_buffer.size() - 1;}
    size_t readableBytes() const;

private:
    std::vector<char> m_buffer;
    std::string m_end;
    std::size_t m_readPos;
    std::size_t m_writePos;
    int Errno;

    size_t writeableBytes() const;
    void writeToBuffer(const char* str, size_t len);
    const char* readAddress() const;
    void ensureWriteable(size_t len, int* Errno);
};