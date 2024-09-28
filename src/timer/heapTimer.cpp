#include "timer/heapTimer.h"

void HeapTimer::addTimer(const TimerNode &t)
{
    m_heap.push_back(t);
    size_t index = m_heap.size() - 1;
    mp[t.id] = index;
    siftUp(index);
}

void HeapTimer::adjustTimer(int id, int newExpires)
{
    if (!mp.count(id)) return;

    size_t index = mp[id];
    m_heap[index].expires = Clock::now() + std::chrono::milliseconds(newExpires);

}

void HeapTimer::delTimer(int id)
{
    if (!mp.count(id)) return;

    size_t index = mp[id];
    delTimer(index);
}

// 删除具体索引的index
void HeapTimer::delTimer(size_t index) {
    if (index >= m_heap.size()) return;

    size_t i = index;
    size_t n = m_heap.size() - 1;
    if (i < n) {
        std::swap(m_heap[i], m_heap[n]);
        mp[m_heap[i].id] = i;
        if (!siftDown(i, n)) {
            siftUp(i);
        }
    }
    mp.erase(m_heap[n].id);
    m_heap.pop_back();
}

void HeapTimer::tick()
{
    if (m_heap.empty()) return;

    TimeStamp now = Clock::now();
    while (!m_heap.empty()) {
        TimerNode node = m_heap.front();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(node.expires - now).count() > 0) {
            break;
        }
        node.fun();
        popTimer();
    }
}

int HeapTimer::getNextTick()
{
    tick();
    if (m_heap.empty()) {
        return -1;
    }
    TimeStamp now = Clock::now();
    int nextTimeout = std::chrono::duration_cast<std::chrono::milliseconds>(m_heap.front().expires - now).count();
    return (nextTimeout > 0 ? nextTimeout : 0);
}

void HeapTimer::popTimer()
{
    if (!m_heap.empty()) {
        delTimer(0);
    }
}

// 堆的向上调整
void HeapTimer::siftUp(size_t i) {
    while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (m_heap[parent] < m_heap[i]) break;
        std::swap(m_heap[i], m_heap[parent]);
        mp[m_heap[i].id] = i;
        mp[m_heap[parent].id] = parent;
        i = parent;
    }
}

// 堆的向下调整
bool HeapTimer::siftDown(size_t index, size_t n) {
    size_t i = index;
    size_t left = 2 * i + 1;
    while (left < n) {
        size_t j = left;
        if (j + 1 < n && m_heap[j + 1] < m_heap[j]) {
            j++;
        }
        if (m_heap[i] < m_heap[j]) break;
        std::swap(m_heap[i], m_heap[j]);
        mp[m_heap[i].id] = i;
        mp[m_heap[j].id] = j;
        i = j;
        left = 2 * i + 1;
    }
    return i > index;
}