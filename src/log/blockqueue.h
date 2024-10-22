#pragma once
#include <deque>
#include <condition_variable>
#include <mutex>
#include <sys/time.h>

template <typename T>
class BlockQueue {
public:
    explicit BlockQueue(size_t maxsize = 1000);
    ~BlockQueue();
    
    void push_back(const T& message);
    bool pop(T& item);
    void clear();
    bool empty();

    // 将存储的消息刷入文件中
    void flush();
    void close();

private:
    std::deque<T> m_deq;
    std::mutex m_mtx;
    bool m_isClose;
    size_t m_capacity;
    std::condition_variable m_condProducer;
    std::condition_variable m_condConsumer;
};

template <typename T>
inline BlockQueue<T>::BlockQueue(size_t maxsize): m_capacity(maxsize), m_isClose(false)
{}

template <typename T>
inline BlockQueue<T>::~BlockQueue()
{
    close();
}

template <typename T>
inline void BlockQueue<T>::push_back(const T &message)
{
    std::unique_lock<std::mutex> lock(m_mtx);
    m_condProducer.wait(lock, [this]() { return m_deq.size() < m_capacity || m_isClose; });

    if (m_isClose) return;
    
    m_deq.push_back(message);
    m_condConsumer.notify_one();
}

template <typename T>
inline bool BlockQueue<T>::pop(T &item)
{
    std::unique_lock<std::mutex> lock(m_mtx);
    m_condConsumer.wait(lock, [this](){ return m_deq.size() || m_isClose; });
    
    if (m_isClose) return false;

    item = m_deq.front();
    m_deq.pop_front();

    m_condProducer.notify_one();
    return true;
}

template <typename T>
inline void BlockQueue<T>::clear()
{
    std::unique_lock<std::mutex> lock(m_mtx);
    m_deq.clear();
}

template <typename T>
inline bool BlockQueue<T>::empty()
{
    std::unique_lock<std::mutex> lock(m_mtx);
    return m_deq.empty();
}

template <typename T>
inline void BlockQueue<T>::flush()
{
    std::unique_lock<std::mutex> lock(m_mtx);
    while (!m_deq.empty()) {
        m_condConsumer.notify_one();  // 强制通知消费
    }
}

template <typename T>
inline void BlockQueue<T>::close()
{
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_isClose = true;
    }
    m_condProducer.notify_all();
    m_condConsumer.notify_all();
}