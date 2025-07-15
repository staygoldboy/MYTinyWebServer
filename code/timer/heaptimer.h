#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h>    // 此包的头文件中定义了inet_ntop()函数，作用为将网络地址转换成字符串
#include <functional>
#include <assert.h>
#include <chrono>
#include <vector>
#include <unordered_map>

#include "../log/log.h"

typedef std::function<void()> TimeoutCallback;
typedef std::chrono::high_resolution_clock Clock;    //clock::now()返回的是当前时间点,类型为time_point,自动进行类型推导
typedef std::chrono::milliseconds MS;  //毫秒
typedef Clock::time_point TimeStamp;


struct TimerNode
{
    int id;
    TimeStamp expires;  //定时器到期时间
    TimeoutCallback cb;  //回调函数
    bool operator<(const TimerNode& t){     //重载<运算符，用于优先级队列的排序
        return expires < t.expires;
    }
    bool operator>(const TimerNode& t){     //重载>运算符，用于优先级队列的排序
        return expires > t.expires;
    }
};

class HeapTimer{
public:
    HeapTimer() {heap_.reserve(64);};  //预留64个空间
    ~HeapTimer() {Clear();};

    void Adjust(int id, int newExpires);
    void Add(int id, int timeout, const TimeoutCallback& cb);
    void DoWork(int id);
    void Clear();
    void Tick();
    void Pop();
    int GetNextTick();


private:
    void Del_(size_t i);
    void SiftUp_(size_t i);
    bool SiftDown_(size_t i, size_t n);
    void SwapNode_(size_t i, size_t j);

    std::vector<TimerNode> heap_;

    std::unordered_map<int, size_t> timerMap_;  //存储id和timerNode在heap_中的索引
};


#endif // HEAP_TIMER_H
