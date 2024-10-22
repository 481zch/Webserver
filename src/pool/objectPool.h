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
    ~ObjectPool();

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(ObjectPool&) = delete;

    Obj* acquireObject();
    void releaseObject(Obj* obj);

private:
    std::queue<Obj*> m_pool;
    std::mutex m_mtx;
    std::condition_variable m_cond;
};


template <typename Obj>
template<typename... Args>
inline ObjectPool<Obj>::ObjectPool(size_t poolSize, Args&&... args)
{
    for (size_t i = 0; i < poolSize; ++ i) {
        m_pool.push(new Obj(std::forward<Args>(args)...));
    }
}

template <typename Obj>
Obj* ObjectPool<Obj>::acquireObject()
{
    std::unique_lock<std::mutex> lock(m_mtx);
    m_cond.wait(lock, [this]() {return !m_pool.empty();});
    
    auto obj = m_pool.front();
    m_pool.pop();
    return obj;
}

template <typename Obj>
void ObjectPool<Obj>::releaseObject(Obj* obj)
{
    std::unique_lock<std::mutex> lock(m_mtx);
    m_pool.push(obj);
    m_cond.notify_one();
}

template <typename Obj>
ObjectPool<Obj>::~ObjectPool()
{
    while (!m_pool.empty()) {
        delete m_pool.front();
        m_pool.pop();
    }
}