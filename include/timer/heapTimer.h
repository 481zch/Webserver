#include <iostream>
#include <algorithm>
#include <vector>
#include <functional>
#include <unordered_map>
#include <time.h>
#include <chrono>

typedef std::function<void()> TimeoutCallback;
typedef std::chrono::high_resolution_clock Clock;
typedef Clock::time_point TimeStamp;

struct TimerNode {
    int id;
    TimeStamp expires;
    TimeoutCallback fun;
    bool operator<(const TimerNode& t) {return expires < t.expires;}
};

class HeapTimer {
public:
    // 默认拷贝构造函数
    void addTimer(const TimerNode& t);
    void adjustTimer(int id, int newExpires);
    void delTimer(int id);
    void delTimer(size_t index);
    void tick();
    int getNextTick();

private:
    std::vector<TimerNode> m_heap;
    // 删除或者修改堆的时候找到对应的节点
    std::unordered_map<int, size_t> mp;

    void popTimer();
    void siftUp(size_t index);
    bool siftDown(size_t index, size_t n);
};
