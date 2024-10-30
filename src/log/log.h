#pragma once
#include <thread>
#include <string>
#include <assert.h>
#include <mutex>
#include <memory>
#include <sstream>
#include <iomanip>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdarg.h>
#include "buffer/linearBuffer.h"
#include "blockqueue.h"

/*
日志应该是线程安全的, 因此使用日志的部分不需要额外确定线程安全
只支持写所有的日志记录，如果想要筛选，请按照格式转换成csv文件在表格中进行筛选
为了高效的日志记录，仅支持异步日志操作，不提供同步日志操作
*/

struct Day {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int seconds;

    bool operator==(const Day& other) {
        return other.year == this->year && other.month == this->month && other.day == this->day;
    }

    bool operator!=(const Day& other) {
        return !(*this == other);
    }

    std::string transToString() {
        std::ostringstream oss;
        oss << std::setw(4) << std::setfill('0') << (year + 1900) << "-"
            << std::setw(2) << std::setfill('0') << (month + 1) << "-"
            << std::setw(2) << std::setfill('0') << day << " "
            << std::setw(2) << std::setfill('0') << hour << ":"
            << std::setw(2) << std::setfill('0') << minute << ":"
            << std::setw(2) << std::setfill('0') << seconds << " ";
        return oss.str();
    }
};

class Log {
public:
    static Log* getInstance();
    static void FlushLogThread(); 
    bool isClosed();
    void write(int level, const char* format, ...);

private:
    LinearBuffer m_buff;
    FILE* m_fp;
    std::unique_ptr<BlockQueue<std::string>> m_deque;
    std::unique_ptr<std::thread> m_writeThread;
    std::mutex m_mtx;
    bool m_isClose;
    int err;

    size_t MAX_LINES;
    size_t m_lineCount;
    Day m_today;
    std::string m_saveDir;
    std::string m_logName;
    std::string m_suffix;

    Log(int maxLines = 50000, std::string saveDir = "/project/webserver/log", std::string suffix = ".log", bool isClose = false);
    ~Log();
    Log(const Log&) = delete;
    Log& operator=(Log&) = delete;
    
    void asyncWrite();
    void appendLogLevelTitle(int level);

    std::string produceFileName();
    void changeFile();
    Day getToday();
};

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);    
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::getInstance();\
        if (!log->isClosed()) {\
            log->write(level, format, ##__VA_ARGS__); \
        }\
    } while(0);
