#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h>   //提供 epoll 相关的系统调用和数据结构
#include <unistd.h>      //提供 close 函数
#include <assert.h>       //提供 assert 函数
#include <vector>       //提供 vector 容器
#include <errno.h>    

class Epoller{
public:
    explicit Epoller(int maxEvent); // explicit 关键字用于禁止隐式类型转换
    ~Epoller();

    bool AddFd(int fd, uint32_t events);
    bool ModFd(int fd, uint32_t events);
    bool DelFd(int fd);
    int Wait(int timeoutMS = -1);
    int GetEventFd(size_t i) const;
    uint32_t GetEvents(size_t i) const;

private:
    int epollFd_; // epoll 文件描述符,epoll_create() 的返回值
    std::vector<struct epoll_event> events_; // 用于存储 epoll_wait() 返回的事件
};


#endif // EPOLLER_H