#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <assert.h>


class ThreadPool {
public:
    ThreadPool() = default;    //默认构造函数
    ThreadPool(ThreadPool&&) = default;    //移动构造函数
    
    explicit ThreadPool(int threadCount = 8) : pool_(std::make_shared<Pool>()) {   //make_shared：传递右值，功能是在动态内存中分配一个对象并初始化它，返回指向此对象的shared_ptr
        assert(threadCount > 0);
        for (int i = 0; i < threadCount; ++i) {
            std::thread([this](){
                std::unique_lock<std::mutex> locker(pool_->mtx_);
                while (true) {
                    if(!pool_->tasks_.empty())
                    {
                        auto task = std::move(pool_->tasks_.front());
                        pool_->tasks_.pop();
                        locker.unlock();  //任务取出后，解锁
                        task();  //执行任务
                        locker.lock();  //执行完任务后，加锁
                    }
                    else if(pool_->isClosed_)
                    {
                        break;
                    }
                    else
                    {
                        pool_->cond_.wait(locker);   //等待任务队列中有任务
                    }
                }
            }).detach();  //分离线程
        }
        
    }

    ~ThreadPool(){
        if(pool_)    //线程池存在
        {
            std::unique_lock<std::mutex> locker(pool_->mtx_);
            pool_->isClosed_ = true;
        }
        pool_->cond_.notify_all();  //唤醒所有线程
    }

    //添加任务
    template<typename T>
    void addTask(T&& task) {       //T&&:万能引用
        std::unique_lock<std::mutex> locker(pool_->mtx_);
        pool_->tasks_.emplace(std::forward<T>(task));  //emplace：在容器中直接构造对象，而不是拷贝或移动 froward：转发参数
        pool_->cond_.notify_one();  //唤醒一个线程
    }


private:
    struct Pool{    ////线程池结构体
        std::mutex mtx_;
        std::condition_variable cond_;  //条件变量
        bool isClosed_;
        std::queue<std::function<void()>> tasks_;   //任务队列
    };

    std::shared_ptr<Pool> pool_;  //线程池
};

#endif // THREADPOOL_H