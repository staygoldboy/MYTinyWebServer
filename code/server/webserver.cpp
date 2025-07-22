#include "webserver.h"

using namespace std;


WebServer::WebServer(
    int port, int trigMode, int timeoutMS, 
    int sqlPort, const char* sqlUser, const char* sqlPwd, const char* sqlDBName,   
    int connPoolNum, int threadNum, 
    bool openLog, int logLevel, int logQueueSize   
): port_(port), timeoutMS_(timeoutMS), isClose_(false), timer_(new HeapTimer()), threadPool_(new ThreadPool(threadNum)), epoller_(new Epoller()) {

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
    srcDir_ = getcwd(NULL, 256); // getcwd(NULL, 256) 获取当前路径, 256为缓冲区大小
    assert(srcDir_);
    // 将srcDir_与"/resources/"拼接
    strcat(srcDir_, "/resources/");  // strcat(srcDir_, "/resources/") 将srcDir_与"/resources/"拼接

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
    // 关闭连接
    client->Close();
    // 将连接从epoller中移除
   epoller_->DelFd(client->GetFd());
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



