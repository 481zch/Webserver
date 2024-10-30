#pragma once

#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <sys/uio.h>

/*
缓冲区不是线程安全的，
使用左闭右开区间表示读写长度
*/

class LinearBuffer {
public:
    LinearBuffer(size_t capacity = 4096);
    ssize_t readFd(int fd, int* Errno);
    ssize_t writeFd(int fd, int* Errno);
    void append(const std::string& str);
    void append(const char* str, size_t len);
    std::string getByEndFlag(const std::string& endFlag);   // 找到结尾符号的第一条数据
    size_t readAbleBytes() const;
    std::string getReadAbleBytes();
    std::string getDataByLength(size_t len);
    std::string justGetData();
    const char* readAddress() {return &m_buffer[m_readPos];}
    void retrieveAll() {bzero(&m_buffer[0], m_buffer.size());m_readPos = m_writePos = 0;}
    void retrieve(size_t len) {m_readPos += len;}
    
private:
    size_t m_readPos;
    size_t m_writePos;
    std::vector<char> m_buffer;
    void writeToBuffer(const char* str, size_t len);
    void expandSpace(size_t len);    // 表示m_buffer扩大到len
    size_t remainCapacity() const;
    void moveTailToHead();
};