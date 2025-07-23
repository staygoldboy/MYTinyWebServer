#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>  // 引入数据类型
#include <sys/uio.h>   // 引入uio结构体,用于读写数据
#include <arpa/inet.h>   // 引入inet_ntoa函数,将网络地址转换为点分十进制字符串
#include <stdlib.h>
#include <errno.h>

#include "../log/log.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"

/*进行读写数据并调用httprequest 来解析数据以及httpresponse来生成响应*/

class HttpConn{
public:
    HttpConn();
    ~HttpConn();

    void Init(int sockFd, const sockaddr_in& addr);  //sockaddr_in结构体用于存储网络地址
    ssize_t Read(int* saveErrno);
    ssize_t Write(int* saveErrno);
    void Close();
    int GetFd() const;
    int GetPort() const;
    const char* GetIP() const;
    sockaddr_in GetAddr() const;
    bool Process();
    
    //获取待写数据长度
    int ToWriteBytes() const{
        return iov_[0].iov_len + iov_[1].iov_len;
    }

    // 判断是否保持连接
    bool IsKeepAlive() const{
        // 调用request_对象的IsKeepAlive()方法
        return request_.IsKeepAlive();
    }

    static bool isET;  //是否为ET模式
    static const char* SrcDir;  //源文件目录
    static std::atomic<int> UserCount;  //用户连接数,使用原子操作

private:
    int fd_;
    struct sockaddr_in addr_;

    bool isClose_;

    int iovCnt_;
    struct iovec iov_[2];  //iovec结构体用于存储读写数据

    Buffer readBuffer_;  //读缓冲区
    Buffer writeBuffer_; //写缓冲区

    HttpRequest request_;  //http请求
    HttpResponse response_;  //http响应
};


#endif // HTTP_CONN_H