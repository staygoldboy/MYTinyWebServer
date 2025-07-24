#include "webserver.h"

using namespace std;


WebServer::WebServer(
    int port, int trigMode, int timeoutMS, 
    int sqlPort, const char* sqlUser, const char* sqlPwd, const char* sqlDBName,   
    int connPoolNum, int threadNum, 
    bool openLog, int logLevel, int logQueueSize): port_(port), timeoutMS_(timeoutMS), isClose_(false), timer_(new HeapTimer()), threadPool_(new ThreadPool(threadNum)), epoller_(new Epoller()) 
{
    // 如果开启日志
    if(openLog){
        // 初始化日志系统
        Log::Instance()->init(logLevel, "./log", ".log", logQueueSize);
        // 如果服务器初始化失败
        if(isClose_){
            LOG_ERROR("============== Server Init Error ==============");
        }
        // 如果服务器初始化成功
        else{
            LOG_INFO("============== Server Init ==============");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",(listenEvent_ & EPOLLET ? "ET" : "LT"), (connEvent_ & EPOLLET ? "ET" : "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::SrcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }

    // 获取当前路径
    srcDir_ = getcwd(NULL, 0); // getcwd(NULL, 256) 获取当前路径, 256为缓冲区大小
    assert(srcDir_);
    // 将srcDir_与"/resources/"拼接
    strcat(srcDir_, "/resources/"); // strcat(srcDir_, "/resources/") 将srcDir_与"/resources/"拼接

    // 设置HttpConn的静态成员变量SrcDir为srcDir_
    HttpConn::SrcDir = srcDir_;
    // 设置HttpConn的静态成员变量UserCount为0
    HttpConn::UserCount = 0;

    // 初始化数据库连接池
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, sqlDBName, connPoolNum);
    // 初始化事件模式
    InitEventMode_(trigMode);
    // 初始化socket
    if(!InitSocket_())
    {
        isClose_ = true;
    }
}


// 析构函数，释放资源
WebServer::~WebServer(){
    // 关闭监听文件描述符
    close(listenFd_);
    // 设置关闭标志
    isClose_ = true;
    // 释放源目录
    free(srcDir_);
    // 关闭数据库连接池
    SqlConnPool::Instance()->ClosePool();
}


/**
事件模式说明：
LT（Level Trigger）：水平触发，事件持续触发直到处理完成
ET（Edge Trigger）：边缘触发，事件只在状态改变时触发一次
EPOLLONESHOT：确保一个socket在任何时刻都只被一个线程处理
**/

// 初始化事件模式
void WebServer::InitEventMode_(int trigMode){
    listenEvent_ = EPOLLRDHUP;  // EPOLLRDHUP：对端关闭连接时，会触发EPOLLIN事件
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;  // EPOLLONESHOT：确保一个socket在任何时刻都只被一个线程处理
    switch(trigMode)
    {
    case 0:  // LT + LT
        break;
    case 1:  // LT + ET
        connEvent_ |= EPOLLET;
        break;
    case 2:  // ET + LT
        listenEvent_ |= EPOLLET;
        break;
    case 3:  // ET + ET
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);
}


// 向客户端发送错误信息
void WebServer::SendError_(int fd, const char* info){
    // 断言fd大于0
    assert(fd > 0);
    // 向客户端发送错误信息
    int ret = send(fd, info, strlen(info), 0);
    // 如果发送失败，则记录警告日志
    if(ret < 0)
    {
        LOG_WARN("send error to client[%d] error!");
    }
    // 关闭fd
    close(fd);
}

// 关闭连接
void WebServer::CloseConn_(HttpConn* client){
    // 断言client指针不为空
    assert(client);
    // 打印日志，显示客户端的文件描述符
    LOG_INFO("Client[%d] quit!", client->GetFd());
    // 将连接从epoller中移除
    epoller_->DelFd(client->GetFd());
    // 关闭连接
    client->Close();
}

// 添加客户端
void WebServer::AddClient_(int fd, sockaddr_in addr){
    assert(fd > 0);  // 断言文件描述符大于0
    clients_[fd].Init(fd, addr);  // 初始化客户端
    if(timeoutMS_ > 0)
    {
        timer_->Add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &clients_[fd]));   // 添加定时器  // std::bind(&WebServer::CloseConn_, this, &clients_[fd])  将CloseConn_函数绑定到this指针和clients_[fd]指针上
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);  // 添加文件描述符到epoller中
    SetFdNonBlock_(fd);  // 设置文件描述符为非阻塞
    LOG_INFO("Client[%d] in!", clients_[fd].GetFd());  // 记录日志
}


// 处理监听事件,EPOLLET 标志表示使用边缘触发模式，必须在一次事件通知中处理完所有就绪的连接，否则可能丢失后续连接
void WebServer::DealListen_(){
    // 定义sockaddr_in结构体变量addr，用于存储客户端地址信息
    struct sockaddr_in addr;
    // 定义socklen_t类型变量len，用于存储addr结构体的大小
    socklen_t len = sizeof(addr);
    // 使用do-while循环，直到listenEvent_与EPOLLET相与的结果为0
    do{
        // 调用accept函数，接受客户端连接，将客户端的文件描述符存储在fd中
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        // 如果fd小于等于0，表示接受连接失败，直接返回
        if(fd <= 0) 
        {
            return;
        }
        // 如果当前连接数大于等于最大连接数，表示服务器忙，向客户端发送错误信息，并记录日志，然后返回
        else if(HttpConn::UserCount >= MAX_FD)
        {
            SendError_(fd, "Internal Server Busy");
            LOG_WARN("Clients are full!");
            return;
        }
        // 调用AddClient_函数，将客户端的文件描述符和地址信息添加到客户端列表中
        AddClient_(fd, addr);
    }while (listenEvent_ & EPOLLET);
}


// 处理读事件，主要逻辑是将OnRead加入线程池的任务队列中
void WebServer::DealRead_(HttpConn* client){
    assert(client);
    ExtentTime_(client);  // 延长定时器时间
    threadPool_->addTask(std::bind(&WebServer::OnRead_, this, client));  // 将OnRead_函数添加到线程池中
}


// 处理写事件，主要逻辑是将OnWrite加入线程池的任务队列中
void WebServer::DealWrite_(HttpConn* client){
    assert(client);
    ExtentTime_(client);  // 延长定时器时间
    threadPool_->addTask(std::bind(&WebServer::OnWrite_, this, client));  // 将OnWrite_函数添加到线程池中
}


// 延长HttpConn对象client的超时时间
void WebServer::ExtentTime_(HttpConn* client){
    // 断言client指针不为空
    assert(client);
    // 如果超时时间大于0
    if(timeoutMS_ > 0)
    {
        // 调整client的文件描述符对应的超时时间
        timer_->Adjust(client->GetFd(), timeoutMS_);
    }
}


void WebServer::OnRead_(HttpConn* client){
    // 断言client指针不为空
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->Read(&readErrno);  // 读取客户端套接字的数据，读到httpconn的读缓存区
    if(ret <= 0 && readErrno != EAGAIN)  // EAGAIN：非阻塞IO，没有数据可读
    {
        CloseConn_(client);  // 关闭连接
        return;
    }
    OnProcess_(client);  // 处理请求
}


/* 处理读（请求）数据的函数 
处理成功：切换为EPOLLOUT（可写事件），准备发送响应数据
处理失败：切换为EPOLLIN（可读事件），继续等待接收数据*/
void WebServer::OnProcess_(HttpConn* client){
    // 如果客户端处理成功
    if(client->Process())  //根据返回的信息重新将fd置为EPOLLIN或EPOLLOUT
    {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);  // 修改文件描述符的监听事件为可写
    }
    else
    {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);  // 修改文件描述符的监听事件为可读
    }
}


// 处理写（响应）数据的函数
void WebServer::OnWrite_(HttpConn* client){
    // 断言client指针不为空
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->Write(&writeErrno);  // 发送数据
    if(client->ToWriteBytes() == 0)
    {
        // 如果还有数据要发送
        if(client->IsKeepAlive())
        {
            //OnProcess_(client);  // 继续处理请求
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);  // 修改文件描述符的监听事件为可读
            return;
        }
    }
    else if(ret < 0)
    {
        // 如果发送失败
        if(writeErrno == EAGAIN)   // EAGAIN：非阻塞IO，没有数据可写
        {
            // 继续发送
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);  // 修改文件描述符的监听事件为可写
            return;
        }
    }
    CloseConn_(client);  // 关闭连接
}

// 初始化套接字
bool WebServer::InitSocket_(){
    int ret = 0;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;  // 使用IPv4地址
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // 设置IP地址为任意地址,监听所有可用网络接口
    addr.sin_port = htons(port_);  // 设置端口号,htons将主机字节序转换为网络字节序
    

    // 创建套接字
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);  // 创建套接字,SOCK_STREAM表示使用TCP协议
    if(listenFd_ < 0)
    {
        LOG_ERROR("Socket create error!");
        return false;
    }
    
    // 设置套接字
    int optval = 1; // 设置套接字选项，允许重用本地地址和端口
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));   // 设置套接字选项,SOL_SOCKET表示使用套接字级别,SO_REUSEADDR允许重用本地地址和端口
    if(ret == -1)
    {
        LOG_ERROR("Set socket setsockopt error!");
        close(listenFd_);
        return false;
    }

    //绑定套接字
    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));  // 绑定套接字
    if(ret < 0)
    {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    // 监听套接字
    ret = listen(listenFd_, 8);  // 监听套接字 8表示最大连接数
    if(ret < 0)
    {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret = epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN);  // 将监听套接字添加到epoll中，监听可读事件
    if(ret == 0)
    {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    SetFdNonBlock_(listenFd_);  // 设置监听套接字为非阻塞
    LOG_INFO("Server port:%d", port_);
    return true;
}

int WebServer::SetFdNonBlock_(int fd){
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK); // 设置文件描述符为非阻塞,fcntl函数用于获取或设置文件描述符的属性
}


// 主事件循环，处理所有事件
void WebServer::Start(){
    int timeMS = -1; 
    if(!isClose_)
    {
        LOG_INFO("=========== Server start! ==========");
    }
    while(!isClose_)
    {
        if(timeoutMS_ > 0)
        {
            timeMS = timer_->GetNextTick();  // 获取下一次的超时等待事件(至少这个时间才会有用户过期，每次关闭超时连接则需要有新的请求进来)
        }
        int eventCnt = epoller_->Wait(timeMS);  // 等待事件发生 
        for(int i = 0; i < eventCnt; i++)
        {
            //处理事件
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            if(fd == listenFd_)   // 如果是监听套接字
            {
                DealListen_();
            }
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))  // 如果是错误事件
            {
                /*处理连接异常情况：
                EPOLLRDHUP: 对端关闭连接或关闭写端
                EPOLLHUP: 连接挂起
                EPOLLERR: 连接出错*/
                assert(clients_.count(fd) > 0);
                CloseConn_(&clients_[fd]);
            }
            else if(events & EPOLLIN)  // 如果是读事件
            {
                assert(clients_.count(fd) > 0);
                DealRead_(&clients_[fd]);  // 处理读事件
            }
            else if(events & EPOLLOUT)  // 如果是写事件
            {
                assert(clients_.count(fd) > 0);
                DealWrite_(&clients_[fd]);  // 处理写事件
            }
            else
            {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}