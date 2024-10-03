#pragma once
#include <iostream>
#include <vector>
#include <unistd.h>

class CircleBuffer {
public:
    // 有一个额外空间是标示位置
    CircleBuffer(int initBufferSize = 4096 + 1, std::string end = "\r\n");
    ssize_t readOut(int fd);
    ssize_t writeIn(int fd, int* Errno);
    void Append(const std::string& str, int* Errno);
    void Append(const char* str, size_t len, int* Errno);
    void Append(const CircleBuffer& buff, int* Errno);
    void Append(const void* data, size_t len, int* Errno);
    std::string getReadableBytes();
    std::string getByEndBytes(int* Errno);

private:
    std::vector<char> m_buffer;
    std::string m_end;
    std::size_t m_readPos;
    std::size_t m_writePos;

    size_t readableBytes() const;
    size_t writeableBytes() const;
    void writeToBuffer(const char* str, size_t len);
    const char* readAddress() const;
    void ensureWriteable(size_t len, int* Errno);
};