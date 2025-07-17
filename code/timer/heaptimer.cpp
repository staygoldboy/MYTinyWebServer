#include "heaptimer.h"

using namespace std;

// 交换堆中的两个节点
void HeapTimer::SwapNode_(size_t i, size_t j) {
    // 断言i和j的值在堆的大小范围内
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    // 交换堆中的两个节点
    swap(heap_[i], heap_[j]);
    // 更新timerMap_中的值
    timerMap_[heap_[i].id] = i;
    timerMap_[heap_[j].id] = j;
}


// 向上调整堆
void HeapTimer::SiftUp_(size_t i) {
    // 断言i在堆的范围内
    assert(i >= 0 && i < heap_.size());
    // 计算父节点的索引
    size_t parent = (i - 1) / 2;
    // 当父节点存在时，继续调整
    while(parent >= 0) {
        // 如果父节点的值大于当前节点的值，交换节点
        if(heap_[parent] > heap_[i]) {
            SwapNode_(i, parent);
            // 更新当前节点和父节点的索引
            i = parent;
            parent = (i - 1) / 2;
        }
        else{
            // 否则，跳出循环
            break;
        }
    }
}

// 向下调整堆
bool HeapTimer::SiftDown_(size_t i, size_t n){
    // 断言i和n的值是否合法
    assert(i >= 0 && i < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    // 将i赋值给index，将2*index+1赋值给child
    auto index = i;
    auto child = 2 * index + 1;
    // 当child小于n时，进行循环
    while(child < n) {
        // 如果child+1小于n且heap_[child+1]小于heap_[child]，则将child+1赋值给child
        if(child + 1 < n && heap_[child + 1] < heap_[child]) {
            ++child;
        }
        // 如果heap_[child]小于heap_[index]，则交换index和child的值，并将index赋值给child，将2*child+1赋值给child
        if(heap_[child] < heap_[index]){
            SwapNode_(index, child);
            index = child;
            child = 2 * index + 1;
        }
        // 否则，跳出循环
        else{
            break;
        }
    }
    return index > i; // 返回是否进行了调整
}


// 删除堆中的节点
void HeapTimer::Del_(size_t index)
{
    // 断言索引在堆的有效范围内
    assert(index >= 0 && index < heap_.size());

    // 临时保存索引
    size_t temp = index;
    // 获取堆的大小
    size_t n = heap_.size() - 1;
    // 断言临时索引在堆的有效范围内
    assert(temp <= n);
    // 如果索引不是最后一个节点
    if(index < n){
        // 交换索引和最后一个节点
        SwapNode_(index, n);
        // 如果向下调整失败
        if(!SiftDown_(temp, n))
        {
            // 向上调整
            SiftUp_(temp);
        }
    }
    // 删除最后一个节点
    timerMap_.erase(heap_.back().id);
    heap_.pop_back();
}

// 调整定时器
void HeapTimer::Adjust(int id, int newExpires){
    // 断言heap_不为空，并且timerMap_中存在id
    assert(!heap_.empty() && timerMap_.find(id) != timerMap_.end());
    // 修改heap_中id对应的定时器的过期时间
    heap_[timerMap_[id]].expires = Clock::now() + MS(newExpires);
    // 调整heap_中id对应的定时器在堆中的位置
    SiftDown_(timerMap_[id], heap_.size());
}


// 向堆定时器中添加一个定时器
void HeapTimer::Add(int id, int timeout, const TimeoutCallback& cb){
    // 断言id大于等于0
    assert(id >= 0);

    // 如果定时器已经存在，则更新定时器的到期时间和回调函数
    if(timerMap_.find(id) != timerMap_.end()){
        int temp = timerMap_[id];
        heap_[temp].expires = Clock::now() + MS(timeout);
        heap_[temp].cb = cb;
        // 如果需要，则向下调整堆
        if(!SiftDown_(temp, heap_.size())){
            SiftUp_(temp);
        }
    }
    // 如果定时器不存在，则添加一个新的定时器
    else{
        size_t n = heap_.size();
        timerMap_[id] = n;
        heap_.push_back({id, Clock::now() + MS(timeout), cb});
        // 向上调整堆
        SiftUp_(n);
    }
}


// 定义HeapTimer类中的DoWork函数，用于执行定时器任务
void HeapTimer::DoWork(int id){
    // 断言heap_不为空且timerMap_中包含id
    assert(!heap_.empty() && timerMap_.find(id) != timerMap_.end());
    // 获取id对应的定时器在heap_中的位置
    size_t i = timerMap_[id];
    // 获取对应的定时器节点
    auto node = heap_[i];
    // 执行定时器任务
    node.cb();
    // 删除定时器
    Del_(i);
}

// 清除堆中的超时定时器
void HeapTimer::Tick(){
    // 如果堆为空，则直接返回
    if(heap_.empty()){
        return;
    }

    // 当堆不为空时，循环执行以下操作
    while(!heap_.empty()){
        // 获取堆顶元素
        TimerNode node = heap_.front();
        // 如果堆顶元素的过期时间减去当前时间的毫秒数大于0，则跳出循环
        if(chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0){
            break;
        }
        // 执行堆顶元素的回调函数
        node.cb();
        // 弹出堆顶元素
        Pop();
    }
}


// 弹出堆顶元素
void HeapTimer::Pop(){
    assert(!heap_.empty());
    Del_(0);
}

// 清除堆中的所有定时器
void HeapTimer::Clear(){
    heap_.clear();
    timerMap_.clear();
}


// 获取下一个定时器的触发时间
int HeapTimer::GetNextTick(){
    // 更新定时器
    Tick();
    // 初始化返回值为-1
    size_t res = -1;
    // 如果定时器队列不为空
    if(!heap_.empty()){
        // 计算定时器队列中第一个定时器的触发时间与当前时间的差值
        res = chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        // 如果差值为负数，则将差值设为0
        if(res < 0){
            res = 0;
        }
    }
    // 返回差值
    return res;
}