#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <deque>
#include <mutex>
#include <condition_variable>
#include <sys/time.h>

using namespace std;

template <typename T>
class BlockQueue
{
public:
    explicit BlockQueue(size_t maxSize = 1000);
    ~BlockQueue();
    bool empty();
    bool full();
    void push_back(const T& item);
    void push_front(const T& item);
    bool pop(T& item);  //弹出的任务放在item中
    bool pop(T& item, int timeout);  // 等待timeout毫秒，如果超时则返回false
    void clear();
    T front();
    T back();
    size_t size();
    size_t capacity();

    void flush();
    void close();
private:
    deque<T> deq_;                       //存储数据的双端队列（仓库本体）
    mutex mtx_;                          //互斥锁，保证线程安全（仓库管理员，确保只有一个人操作）
    bool isClosed_;                      //是否关闭（仓库是否停业）
    size_t capacity_;                    //仓库容量
    condition_variable condConsumer_;    //消费者等待条件变量（通知取货员有货了）
    condition_variable condProducer_;      //生产者等待条件变量（通知送货员有空间了）

};

template <typename T>
BlockQueue<T>::BlockQueue(size_t maxSize): capacity_(maxSize){
    assert(maxSize > 0);
    isClosed_ = false;
}

template <typename T>
BlockQueue<T>::~BlockQueue(){
    close();
}

template <typename T>
void BlockQueue<T>::close(){
    clear();
    isClosed_ = true;
    condConsumer_.notify_all();   //通知所有消费者
    condProducer_.notify_all();   //通知所有生产者
}

template <typename T>
void BlockQueue<T>::clear(){
    lock_guard<mutex> locker(mtx_);
    deq_.clear();
}

template <typename T>
bool BlockQueue<T>::empty(){
    lock_guard<mutex> locker(mtx_);
    return deq_.empty();
}

template <typename T>
bool BlockQueue<T>::full(){
    lock_guard<mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

template <typename T>
void BlockQueue<T>::push_back(const T& item){
    unique_lock<mutex> locker(mtx_);  //unique_lock比lock_guard更灵活，可以手动解锁,条件变量需要搭配unique_lock使用
    while(deq_.size() >= capacity_)    //如果仓库满了，则等待消费者消费
    {
        condProducer_.wait(locker);   //暂停生产，等待消费者唤醒生产条件变量
    }
    deq_.push_back(item);
    condConsumer_.notify_one();   //通知消费者有货了
}

template <typename T>
void BlockQueue<T>::push_front(const T& item){
    unique_lock<mutex> locker(mtx_);
    while(deq_.size() >= capacity_)    //如果仓库满了，则等待消费者消费
    {
        condProducer_.wait(locker);   //暂停生产，等待消费者唤醒生产条件变量
    }
    deq_.push_front(item);
    condConsumer_.notify_one();   //通知消费者有货了
}

template <typename T>
bool BlockQueue<T>::pop(T& item){
    unique_lock<mutex> locker(mtx_);
    while(deq_.empty())  //如果仓库空了，则等待生产者生产
    {
        if(isClosed_)    //如果仓库停业了，则返回false
        {
            return false;
        }
        condConsumer_.wait(locker);    //暂停消费，等待生产者唤醒消费条件变量
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();   //通知生产者有空间了
    return true;
}

template <typename T>
bool BlockQueue<T>::pop(T& item, int timeout){
    unique_lock<mutex> locker(mtx_);
    while(deq_.empty())  //如果仓库空了，则等待生产者生产
    {
        if(condConsumer_.wait_for(locker, chrono::second(timeout)) == cv_status::timeout) //等待timeout秒，如果超时则返回false
        {
            return false;
        }
        if(isClosed_)
        {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();   //通知生产者有空间了
    return true;
}

template <typename T>
T BlockQueue<T>::front(){
    lock_guard<mutex> locker(mtx_);
    return deq_.front();
}

template <typename T>
T BlockQueue<T>::back(){
    lock_guard<mutex> locker(mtx_);
    return deq_.back();
}

template <typename T>
size_t BlockQueue<T>::size(){
    lock_guard<mutex> locker(mtx_);
    return deq_.size();
}

template <typename T>
size_t BlockQueue<T>::capacity(){
    lock_guard<mutex> locker(mtx_);
    return capacity_;
}

template <typename T>
void BlockQueue<T>::flush(){
    condConsumer_.notify_one();   //通知消费者有货了
}

#endif // BLOCKQUEUE_H