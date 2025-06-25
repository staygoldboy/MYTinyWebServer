#include "buffer.h"

//读写位置初始化为0
Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

size_t Buffer::ReadableBytes() const {
    return writePos_ - readPos_;
}

size_t Buffer::WritableBytes() const {
    return buffer_.size() - writePos_;
}

size_t Buffer::RecyclableBytes() const {
    return readPos_;
}

const char* Buffer::Peek() const {
    return &buffer_[readPos_];
}

void Buffer::Retrieve(size_t len) {
    readPos_ += len;
}

void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end);
    Retrieve(end - Peek()); //Peek()返回的是const char*，所以这里需要减去const char*类型
}

void Buffer::RetrieveAll(){
    bzero(&buffer_[0], buffer_.size()); //将buffer_中的数据全部置为0
    readPos_ = 0;
    writePos_ = 0;
}

std::string Buffer::RetrieveAllToStr(){
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

const char* Buffer::BeginWritePtr() const {
    return &buffer_[writePos_]; 
}

char* Buffer::BeginWrite() {
    return &buffer_[writePos_]; 
}

void Buffer::HasWritten(size_t len) {
    writePos_ += len;
}

void Buffer::EnsureWritableBytes(size_t len) {
    if(len > WritableBytes()) {
        MakeSpace_(len);
    }
    assert(len <= WritableBytes());
}

void Buffer::Append(const char* str, size_t len) {
    assert(str);
    EnsureWritableBytes(len);
    std::copy(str, str + len, BeginWrite());
    HasWritten(len);
}

void Buffer::Append(const std::string& str) {
    Append(str.c_str(), str.size());
}

void Buffer::Append(const void* data, size_t len) {
    Append(static_cast<const char*>(data), len);
}

void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}

//将fd中的数据读入buffer_中
ssize_t Buffer::ReadFd(int fd, int* Errno){
    char buff[65535];  //65535是Linux系统默认的缓冲区大小
    struct iovec iov[2];  //iovec是Linux系统定义的一个结构体，用于描述一个缓冲区
    size_t writable = WritableBytes(); //获取buffer_中可写的大小
    iov[0].iov_base = BeginWrite();  //第一块：iov_base指向缓冲区的起始地址
    iov[0].iov_len = writable;  //iov_len指向缓冲区的长度
    iov[1].iov_base = buff;    //第二块：iov_base指向一个临时的缓冲区
    iov[1].iov_len = sizeof(buff);   //iov_len指向临时的缓冲区的长度

    ssize_t len = readv(fd, iov, 2);  //将fd中的数据读入iov中
    if(len < 0) {
        *Errno = errno;
    }else if(static_cast<size_t>(len) <= writable) {  //如果读入的数据小于等于buffer_中可写的大小
        writePos_ += len;  //将写位置向后移动len个字节
    }else {  //如果读入的数据大于buffer_中可写的大小
        writePos_ = buffer_.size();  //将写位置置为buffer_的末尾
        Append(buff,static_cast<size_t>(len) - writable);  //将读入的数据中剩余的部分追加到buffer_中
    }
    return len;
}

ssize_t Buffer::WriteFd(int fd, int* Errno) {
    ssize_t len = write(fd, Peek(), ReadableBytes());
    if(len < 0) {
        *Errno = errno;
        return len;
    }
    Retrieve(len);
    return len;
}

char* Buffer::BeginPtr_() {
    return &buffer_[0];
}

const char* Buffer::BeginPtr_() const {
    return &buffer_[0];
}

//扩展buffer_的大小
void Buffer::MakeSpace_(size_t len) {
    if(WritableBytes() + RecyclableBytes() < len) {//如果buffer_中可写的大小加上可回收的大小小于len，则扩展buffer_的大小
        buffer_.resize(writePos_ + len + 1);
    }else { //如果buffer_中可写的大小加上可回收的大小大于等于len，则将可读的数据移动到buffer_的开头
        size_t readable = ReadableBytes();
        std::copy(BeginPtr_() + readPos_,BeginPtr_() + writePos_, BeginPtr_());  //将可读的数据移动到buffer_的开头
        readPos_ = 0;
        writePos_ = readable;
        assert(readable == ReadableBytes());
    }
}