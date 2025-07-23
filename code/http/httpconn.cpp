#include "httpconn.h"
using namespace std;


const char* HttpConn::SrcDir;
std::atomic<int> HttpConn::UserCount;
bool HttpConn::isET;

HttpConn::HttpConn(){
    fd_ = -1;
    addr_ = {0};
    isClose_ = true;
}

HttpConn::~HttpConn(){
    Close();
}

//初始化连接
void HttpConn::Init(int fd, const sockaddr_in& addr){
    assert(fd > 0);
    UserCount++;
    fd_ = fd;
    addr_ = addr;
    writeBuffer_.RetrieveAll();
    readBuffer_.RetrieveAll();
    isClose_ = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)UserCount);
}

//关闭连接
void HttpConn::Close(){
    response_.UnmapFile(); //关闭文件映射
    if(isClose_ == false){
        isClose_ = true;
        UserCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) close, userCount:%d", fd_, GetIP(), GetPort(), (int)UserCount);
    }
}

//返回文件描述符
int HttpConn::GetFd() const{
    return fd_;
}


// 获取HttpConn的地址
struct sockaddr_in HttpConn::GetAddr() const{
    // 返回addr_成员变量
    return addr_;
}

// 获取IP地址
const char* HttpConn::GetIP() const{
    // 将addr_中的sin_addr转换为字符串形式
    return inet_ntoa(addr_.sin_addr);   
}


// 获取端口号
int HttpConn::GetPort() const{
    // 返回addr_中的sin_port
    return ntohs(addr_.sin_port);
}

// 从文件描述符中读取数据，存储到readBuffer_中
ssize_t HttpConn::Read(int* saveErrno){
    // 定义变量len，用于存储读取的字节数
    ssize_t len = -1;
    // 循环读取数据
    do{
        // 从文件描述符fd_中读取数据，存储到readBuffer_中，并返回读取的字节数
        len = readBuffer_.ReadFd(fd_, saveErrno);
        // 如果读取的字节数小于等于0，则跳出循环
        if(len <= 0){
            break;
        }
    // 如果isET为true，则继续循环
    }while(isET);
    return len;
}


// 将数据从iov_中写入文件描述符fd_
ssize_t HttpConn::Write(int* saveErrno){
    ssize_t len = -1;
    do{
        len = writev(fd_, iov_, iovCnt_);   //将iov_中的数据写入文件描述符fd_
        if(len <= 0){
            *saveErrno = errno;
            break;
        }
        if(iov_[0].iov_len + iov_[1].iov_len == 0){    //传输完成
            break;
        }
        else if(static_cast<size_t>(len) > iov_[0].iov_len){      //iov_[0]传输完成
            iov_[1].iov_base = (uint8_t*)iov_[1].iov_base + (len - iov_[0].iov_len);    //更新iov_[1]的iov_base指针
            iov_[1].iov_len -= (len - iov_[0].iov_len);    //更新iov_[1]的iov_len
            if(iov_[0].iov_len){
                writeBuffer_.RetrieveAll();    //将iov_[0]中的数据从writeBuffer_中移除
                iov_[0].iov_len = 0;
            }
        }
        else{
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len;    //更新iov_[0]的iov_base指针
            iov_[0].iov_len -= len;    //更新iov_[0]的iov_len
            writeBuffer_.Retrieve(len);    //将iov_[0]中的数据从writeBuffer_中移除
        }
    }while(isET || ToWriteBytes() > 10240);
    return len;
}

// 处理连接
bool HttpConn::Process(){
    // 初始化request_
    request_.Init();
    // 如果readBuffer_中没有可读字节，返回false
    if(readBuffer_.ReadableBytes() <= 0){
        return false;
    }
    // 如果request_解析成功，初始化response_
    else if(request_.parse(readBuffer_)){
        LOG_DEBUG("%s",request_.path().c_str());
        response_.Init(SrcDir, request_.path(), request_.IsKeepAlive(), 200);
    }
    // 如果request_解析失败，初始化response_，状态码为400
    else{
        response_.Init(SrcDir, request_.path(), false, 400);
    }

    // 生成response_的响应
    response_.MakeResponse(writeBuffer_);  
    // 将writeBuffer_中的数据放入iov_中
    iov_[0].iov_base = const_cast<char*>(writeBuffer_.Peek());
    iov_[0].iov_len = writeBuffer_.ReadableBytes();
    iovCnt_ = 1;

    // 如果response_中有文件，将文件放入iov_中
    if(response_.FileLen() > 0 && response_.File()){
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }


    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());
    // 返回true
    return true;
}
