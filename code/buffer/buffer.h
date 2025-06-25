#ifndef BUFFER_H
#define BUFFER_H

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/uio.h>
#include <vector>
#include <atomic>
#include <assert.h>


class Buffer
{ 
public:
    Buffer(int initBufSize = 1024);
    ~Buffer() = default;

    //返回可写空间大小 
    size_t WritableBytes() const;
    //返回可读空间大小
    size_t ReadableBytes() const;
    //返回可回收空间大小
    size_t RecyclableBytes() const;

    /*数据读取操作*/
    //返回当前可读数据的起始指针
    const char* Peek() const;
    //读取len个字节的数据
    void Retrieve(size_t len);
    //读取直到end为止的数据
    void RetrieveUntil(const char* end);
    //读取所有数据
    void RetrieveAll();
    //将所有可读数据转换为字符串，并清空缓冲区
    std::string RetrieveAllToStr();

    /*数据写入操作*/
    //返回当前可写数据的起始指针
    const char* BeginWritePtr() const;
    char* BeginWrite();
    //写入len个字节的数据
    void Append(const char* data, size_t len);
    //写入字符串
    void Append(const std::string& str);
    //写入自定义数据
    void Append(const void* data, size_t len);
    //将buf中的数据写入
    void Append(const Buffer& buf);
    //通知已写入指定长度数据，移动写指针
    void HasWritten(size_t len);
    //确保可写空间大小，如果不足则扩容
    void EnsureWritableBytes(size_t len);

    ssize_t ReadFd(int fd, int* Errno);
    ssize_t WriteFd(int fd, int* Errno);

private:
    char* BeginPtr_();
    const char* BeginPtr_() const;
    void MakeSpace_(size_t len);

    std::vector<char> buffer_;
    std::atomic<size_t> readPos_; //当前可读数据的起始指针
    std::atomic<size_t> writePos_;  //当前可写数据的起始指针
};



#endif // !BUFFER_H