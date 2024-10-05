#include <gtest/gtest.h>
#include <future>
#include "pool/threadPool.h"

// 测试任务提交和任务执行
TEST(ThreadPoolTest, TaskExecution) {
    ThreadPool pool(4);  // 初始化线程池，设定基础线程数为4
    pool.init();

    // 提交一个简单的任务并获取结果
    auto result = pool.submit(1, []() { return 42; });
    
    // 检查任务执行是否成功
    ASSERT_EQ(result.get(), 42);
    
    pool.shutdown();
}

// 测试多个任务的并行执行
TEST(ThreadPoolTest, MultipleTasksExecution) {
    ThreadPool pool(4);  // 初始化线程池，设定基础线程数为4
    pool.init();

    std::vector<std::future<int>> results;

    // 提交多个任务
    for (int i = 0; i < 10; ++i) {
        results.emplace_back(pool.submit(1, [i]() { return i * i; }));
    }

    // 检查任务执行结果
    for (int i = 0; i < 10; ++i) {
        ASSERT_EQ(results[i].get(), i * i);
    }

    pool.shutdown();
}

// 测试线程池的动态调整
TEST(ThreadPoolTest, DynamicThreadResize) {
    ThreadPool pool(2, 2, 8);  // 最小线程数为2，最大线程数为8
    pool.init();

    std::vector<std::future<void>> results;

    // 提交多个任务来测试线程池动态扩展
    for (int i = 0; i < 20; ++i) {
        results.emplace_back(pool.submit(1, []() { std::this_thread::sleep_for(std::chrono::milliseconds(500)); }));
    }

    pool.shutdown();

    // 确保所有任务都能正确完成
    for (auto& result : results) {
        result.get();  // 确保所有任务完成
    }
}

// 测试任务优先级处理
TEST(ThreadPoolTest, TaskPriority) {
    ThreadPool pool(4);  // 初始化线程池，设定基础线程数为4
    pool.init();

    // 提交不同优先级的任务
    std::vector<int> result;

    auto highPriorityTask = pool.submit(10, [&result]() {
        result.push_back(10);
    });

    auto lowPriorityTask = pool.submit(1, [&result]() {
        result.push_back(1);
    });

    highPriorityTask.get();
    lowPriorityTask.get();

    // 高优先级任务应先执行
    ASSERT_EQ(result[0], 10);
    ASSERT_EQ(result[1], 1);

    pool.shutdown();
}