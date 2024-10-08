#pragma once

#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>

/*
此对象池用于连接对象创建时的复用工作
现在使用单例模式的饿汉式来保证线程安全
后期可以考虑动态的伸缩这个对象池
*/

const int defaultSize = 50;

template <typename Obj>
class ObjectPool{
public:
    template<typename... Args>
    ObjectPool(size_t poolSize = defaultSize, Args&&... args);

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(ObjectPool&) = delete;

    std::unique_ptr<Obj> acquireObject();
    void releaseObject(std::unique_ptr<Obj> obj);

private:
    std::queue<std::unique_ptr<Obj>> m_pool;
    std::mutex m_mtx;
    std::condition_variable m_cond;
};


template <typename Obj>
template<typename... Args>
inline ObjectPool<Obj>::ObjectPool(size_t poolSize, Args&&... args)
{
    for (size_t i = 0; i < poolSize; ++ i) {
        m_pool.push(std::make_unique<Obj>(std::forward<Args>(args)...));
    }
}

template <typename Obj>
std::unique_ptr<Obj> ObjectPool<Obj>::acquireObject()
{
    std::unique_lock<std::mutex> lock(m_mtx);
    m_cond.wait(lock, [this]() {return !m_pool.empty();});
    
    std::unique_ptr<Obj> obj = std::move(m_pool.front());
    m_pool.pop();
    return obj;
}

template <typename Obj>
void ObjectPool<Obj>::releaseObject(std::unique_ptr<Obj> obj)
{
    std::unique_lock<std::mutex> lock(m_mtx);
    m_pool.push(std::move(obj));
    m_cond.notify_one();
}