#include <gtest/gtest.h>
#include <fcntl.h>
#include <unistd.h>
#include "log/log.h"
#include <chrono>
#include <thread>

// 模拟多线程写日志任务，增加更多日志信息和写入次数
void writeLogTask(Log* log, int threadId, int logCount) {
    for (int i = 0; i < logCount; ++i) {
        log->LOG_RECORD(0, "Thread %d is writing log %d", threadId, i);
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 模拟写日志的延迟
    }
}

TEST(LogTest, TestLogMultipleThreads) {
    Log* log = Log::getInstance();

    // 创建8个线程，每个线程写入200条日志
    const int threadCount = 8;
    const int logCountPerThread = 200;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back(writeLogTask, log, i + 1, logCountPerThread);
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
}

// 测试日志文件切换，达到最大行数后自动切换日志文件
TEST(LogTest, TestLogFileRotation) {
    Log* log = Log::getInstance();

    const int logCount = 60000;  // 模拟足够多的日志行数，确保日志文件会切换

    for (int i = 0; i < logCount; ++i) {
        log->LOG_RECORD(0, "Writing log %d", i);
        if (i % 1000 == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));  // 模拟间歇性日志写入
        }
    }
}
