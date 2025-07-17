#include "epoller.h"


// 构造函数，初始化Epoller对象
Epoller::Epoller(int maxEvent):epollFd_(epoll_create(512)), events_(maxEvent) {
    // 断言epollFd_大于等于0，并且events_的大小大于0
    assert(epollFd_ >= 0 && events_.size() > 0);
}

// 析构函数，关闭epollFd_
Epoller::~Epoller() {
    close(epollFd_);
}


// 添加文件描述符到Epoller中
bool Epoller::AddFd(int fd, uint32_t events) {
    // 如果文件描述符小于0，则返回false
    if(fd < 0){
        return false;
    }
    // 创建epoll_event结构体
    epoll_event ev = {0};
    // 设置文件描述符
    ev.data.fd = fd;
    // 设置事件类型
    ev.events = events;
    // 将文件描述符添加到Epoller中
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
}


// 修改文件描述符的事件
bool Epoller::ModFd(int fd, uint32_t events) {
    // 如果文件描述符小于0，返回false
    if(fd < 0){
        return false;
    }
    // 定义一个epoll_event结构体
    epoll_event ev = {0};
    // 设置文件描述符
    ev.data.fd = fd;
    // 设置事件
    ev.events = events;
    // 调用epoll_ctl函数，修改文件描述符的事件
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}


// 删除文件描述符
bool Epoller::DelFd(int fd) {
    // 如果文件描述符小于0，返回false
    if(fd < 0){
        return false;
    }
    // 调用epoll_ctl函数，删除文件描述符
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, 0);
}


// 等待事件发生
int Epoller::Wait(int timeoutMs) {
    // 调用epoll_wait函数，等待事件发生
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
}


// 获取第i个事件的文件描述符
int Epoller::GetEventFd(size_t i) const {
    // 断言i在events_的范围内
    assert(i < events_.size() && i >= 0);
    // 返回第i个事件的文件描述符
    return events_[i].data.fd;
}


// 获取第i个事件的事件类型
uint32_t Epoller::GetEvents(size_t i) const {
    // 断言i在events_的范围内
    assert(i < events_.size() && i >= 0);
    // 返回第i个事件的事件类型
    return events_[i].events;
}