#include <gtest/gtest.h>
#include "pool/objectPool.h"  // 假设头文件名为 object_pool.h
#include <thread>
#include <vector>

class TestObject {
public:
    TestObject() = default;
    ~TestObject() = default;
    void doSomething() {
        // 测试对象中的一些行为
    }
};

TEST(ObjectPoolTest, AcquireReleaseObject) {
    // 获取对象池实例
    auto* pool = ObjectPool<TestObject>::getInstance();

    // 从对象池中获取一个对象
    auto obj = pool->acquireObject();
    
    // 确保对象不是空的
    ASSERT_NE(obj, nullptr);

    // 调用对象的行为来确保其正常工作
    obj->doSomething();

    // 归还对象
    pool->releaseObject(std::move(obj));

    // 再次获取对象，检查对象池正常复用对象
    auto obj2 = pool->acquireObject();
    ASSERT_NE(obj2, nullptr);
}

TEST(ObjectPoolTest, MultiThreadAcquireRelease) {
    // 获取对象池实例
    auto* pool = ObjectPool<TestObject>::getInstance();

    // 创建多个线程并从对象池获取对象
    const int threadCount = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([pool]() {
            auto obj = pool->acquireObject();
            ASSERT_NE(obj, nullptr);
            obj->doSomething();
            pool->releaseObject(std::move(obj));
        });
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
}
