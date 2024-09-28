#pragma once

#include <list>
#include <mutex>
#include <queue>
#include <functional>
#include <future>
#include <thread>
#include <utility>
#include <vector>
#include <iostream>
#include <condition_variable>
#include <initializer_list>
#include <unordered_map>
#include <atomic>
#include <chrono>

#define MIN_THREADS 4
#define MAX_THREADS 40
#define DEFAULT_THREADS 4
#define DEFAULT_INTERVAL 1000

// Task类声明
class Task {
public:
    std::function<void()> func;
    unsigned int priority;

public:
    Task();
    Task(std::function<void()> f, unsigned int p);

    bool operator<(const Task& other) const;
};

// SafeQueue类声明
template <typename T>
class SafeQueue {
private:
    std::priority_queue<T, std::vector<T>, std::less<T>> m_queue;
    std::mutex m_mutex;

public:
    SafeQueue();
    SafeQueue(SafeQueue&& other);
    ~SafeQueue();

    bool empty();
    int size();
    void enqueue(T&& t);
    bool dequeue(T& t);
};

// 线程池类声明
class ThreadPool {
private:
    class ThreadWorker {
    private:
        ThreadPool* m_pool;

    public:
        ThreadWorker(ThreadPool* pool);
        void operator()();
    };

    bool m_shutdown;
    SafeQueue<Task> m_queue;
    std::mutex m_conditional_mutex;
    std::mutex m_mutex;
    std::condition_variable m_conditional_lock;
    size_t max_threads;
    size_t min_threads;

    std::unordered_map<std::thread::id, std::list<std::thread>::iterator> mp;
    std::list<std::thread> lst_threads;
    std::atomic<int> work_nums;
    std::atomic<int> sleep_nums;

    std::chrono::milliseconds timer_interval;
    std::thread timer_thread;

public:
    ThreadPool(const int n_threads = DEFAULT_THREADS, const size_t min_threads = MIN_THREADS, const size_t max_threads = MAX_THREADS, std::chrono::milliseconds interval = std::chrono::milliseconds(DEFAULT_INTERVAL));
    ~ThreadPool();

    void init();
    void shutdown();

    template <typename F, typename... Args>
    auto submit(unsigned int priority, F&& f, Args&&... args) -> std::future<decltype(f(args...))>;

    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;

private:
    void resizePool();
    void timer_function();
};


// Task类定义
Task::Task() = default;

Task::Task(std::function<void()> f, unsigned int p) : func(std::move(f)), priority(p) {}

bool Task::operator<(const Task& other) const {
    return priority < other.priority;
}

// SafeQueue类定义
template <typename T>
SafeQueue<T>::SafeQueue() {}

template <typename T>
SafeQueue<T>::SafeQueue(SafeQueue&& other) {
    std::lock_guard<std::mutex> lock(other.m_mutex);
    m_queue = std::move(other.m_queue);
}

template <typename T>
SafeQueue<T>::~SafeQueue() {}

template <typename T>
bool SafeQueue<T>::empty() {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_queue.empty();
}

template <typename T>
int SafeQueue<T>::size() {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_queue.size();
}

template <typename T>
void SafeQueue<T>::enqueue(T&& t) {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_queue.push(std::forward<T>(t));
}

template <typename T>
bool SafeQueue<T>::dequeue(T& t) {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_queue.empty())
        return false;
    t = std::move(m_queue.top());
    m_queue.pop();
    return true;
}

// ThreadPool::ThreadWorker类定义
ThreadPool::ThreadWorker::ThreadWorker(ThreadPool* pool) : m_pool(pool) {}

void ThreadPool::ThreadWorker::operator()() {
    Task t;
    bool dequeued;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(m_pool->m_conditional_mutex);
            m_pool->m_conditional_lock.wait(lock, [this] { return !m_pool->m_queue.empty() || m_pool->m_shutdown; });
            if (m_pool->m_shutdown && m_pool->m_queue.empty()) break;
            dequeued = m_pool->m_queue.dequeue(t);
        }
        if (dequeued) {
            ++m_pool->work_nums;
            --m_pool->sleep_nums;
            t.func();
            --m_pool->work_nums;
            ++m_pool->sleep_nums;
        }
        else {
            std::unique_lock<std::mutex> lock(m_pool->m_mutex);
            std::thread::id this_id = std::this_thread::get_id();
            if (m_pool->mp.count(this_id)) {
                m_pool->lst_threads.erase(m_pool->mp[this_id]);
                m_pool->mp.erase(this_id);
                --m_pool->sleep_nums;
            }
            return;
        }
    }
}

// ThreadPool类定义
ThreadPool::ThreadPool(const int n_threads, const size_t min_threads, const size_t max_threads, std::chrono::milliseconds interval)
    : lst_threads(std::list<std::thread>(n_threads)), m_shutdown(false), min_threads(min_threads), max_threads(max_threads), timer_interval(interval) {
    work_nums = 0;
    sleep_nums = n_threads;
    timer_thread = std::thread(&ThreadPool::timer_function, this);
}

ThreadPool::~ThreadPool() {
    if (!m_shutdown) shutdown();
}

void ThreadPool::init() {
    for (auto it = lst_threads.begin(); it != lst_threads.end(); ++it) {
        *it = std::thread(ThreadWorker(this));
        mp[it->get_id()] = it;
    }
}

void ThreadPool::shutdown() {
    {
        std::unique_lock<std::mutex> lock(m_conditional_mutex);
        m_shutdown = true;
    }
    m_conditional_lock.notify_all();
    for (auto& it : lst_threads) {
        if (it.joinable()) {
            it.join();
        }
    }
    if (timer_thread.joinable()) {
        timer_thread.join();
    }
}

void ThreadPool::resizePool() {
    std::unique_lock<std::mutex> lock(m_mutex);
    size_t task_num = m_queue.size();
    size_t cur_threads = lst_threads.size();

    // 扩展线程池
    if (task_num > work_nums && cur_threads < max_threads) {
        size_t add_threads = std::min(max_threads - cur_threads, task_num - work_nums);
        for (size_t i = 0; i < add_threads; ++i) {
            auto it = lst_threads.emplace(lst_threads.end(), std::thread(ThreadWorker(this)));
            mp[it->get_id()] = it;
            ++sleep_nums;
        }
        return;
    }

    // 收缩线程池
    if (work_nums < sleep_nums && cur_threads > min_threads) {
        size_t remove_threads = cur_threads - std::max(2 * task_num, (size_t)min_threads);
        size_t temp = std::min((size_t)sleep_nums, cur_threads - min_threads);
        remove_threads = std::min(temp, remove_threads);

        for (size_t i = 0; i < remove_threads; ++i) {
            m_conditional_lock.notify_one();
        }
        return;
    }
}

void ThreadPool::timer_function() {
    while (!m_shutdown) {
        std::this_thread::sleep_for(timer_interval);
        resizePool();
    }
}

