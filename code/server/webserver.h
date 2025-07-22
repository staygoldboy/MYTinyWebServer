#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>   //这个库的头文件中定义了文件控制相关的函数
#include <unistd.h>   
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>     //这个库的头文件中定义了socket相关的函数
#include <netinet/in.h>    //这个库的头文件中定义了IPv4和IPv6相关的结构体
#include <arpa/inet.h>      //这个库的头文件中定义了inet_pton和inet_ntop函数

#include "epoller.h"
#include "../timer/heaptimer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../http/httpconn.h"

class WebServer{
public:
    WebServer(
        int port, int trigMode, int timeoutMS,    //端口号，触发模式，超时时间
        int sqlPort, const char* sqlUser, const char* sqlPwd, const char* sqlDBName,   //数据库信息
        int connPoolNum, int threadNum,  //连接池和线程池数量
        bool openLog, int logLevel, int logQueueSize   //日志信息
    );

    ~WebServer();
    void Start();

private:
    bool InitSocket_();
    void InitEventMode_(int trigMode);
    void AddClient_(int fd, sockaddr_in clientAddr);

    void DealListen_();   //处理listen事件
    void DealWrite_(HttpConn* client);
    void DealRead_(HttpConn* client);

    void SendError_(int fd, const char* info);  //发送错误信息
    void ExtentTime_(HttpConn* client);  //延长超时时间
    void CloseConn_(HttpConn* client);  //关闭连接

    void OnRead_(HttpConn* client);  //处理读事件
    void OnWrite_(HttpConn* client);  //处理写事件
    void OnProcess_(HttpConn* client);  //处理业务

    static const int MAX_FD = 65536;  //最大文件描述符数量

    static int SetFdNonBlock_(int fd);  //将文件描述符设置为非阻塞

    int port_;  //端口号
    bool openLinger_;  //是否开启延迟关闭
    int timeoutMS_;  //超时时间
    bool isClose_;  //是否关闭
    int listenFd_;  //监听文件描述符
    char* srcDir_;  //网页资源目录

    uint32_t listenEvent_;  //监听事件模式（LT/ET）
    uint32_t connEvent_;  //连接事件模式（LT/ET）

    std::unique_ptr<HeapTimer> timer_;  //定时器
    std::unique_ptr<ThreadPool> threadPool_;  //线程池
    std::unique_ptr<Epoller> epoller_;  //epoll对象

    std::unordered_map<int, HttpConn> clients_;  //客户端连接
};




#endif // WEBSERVER_H