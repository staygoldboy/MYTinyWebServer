#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/stat.h>
#include "blockqueue.h"
#include "../buffer/buffer.h"

class Log{
public:
    void init(int level,const char* path = "./log",const char* suffix = ".log",int maxQueueCapacity = 1024);   //初始化日志实例（设置日志级别，日志保存路径，日志文件后缀，阻塞队列最大容量）
    static Log* Instance();   //获取日志实例
    static void FlushLogThread();   //异步刷新日志线程

    void WriteLog(int level,const char* format,...);   //写日志
    void Flush();   //刷新日志

    int GetLevel();   //获取日志级别
    void SetLevel(int level);   //设置日志级别

    bool IsOpen() {return isOpen_;}   //判断日志是否打开


private:
    Log();   //构造函数私有化，禁止外部创建实例
    virtual ~Log();
    void AsynWriteLog();   //异步写日志
    void AppendLogLevelTitle(int level);   //追加日志级别

private:
    static const int LOG_PATH_LEN = 256;   //日志路径长度
    static const int LOG_NAME_LEN = 256;   //日志文件名长度
    static const int MAX_LINES = 50000;    //日志文件最大行数

    const char* path_;   //日志保存路径
    const char* suffix_;   //日志文件后缀

    int MAX_LINES_;   //日志文件最大行数
    int lineCount_;   //日志文件当前行数
    int toDay_;   //日志文件当前日期

    bool isOpen_;   //日志是否打开

    Buffer buff_;   //日志缓冲区
    int level_;   //日志级别
    bool isAsync_;   //是否异步写日志

    FILE* fp_;    //日志文件指针
    std::unique_ptr<BlockQueue<std::string>> deque_;   //阻塞队列
    std::unique_ptr<std::thread> writeThread_;   //写日志线程
    std::mutex mtx_;   //互斥锁

};

#define LOG_BASE(level, format, ...)\
    do{\
        Log* log = Log::Instance();\
        if(log->isOpen() && log->GetLevel() <= level){\
            log->WriteLog(level, format, ##__VA_ARGS__);\
            log->Flush();\
        }\
    }while(0);

//四个宏定义，分别对应debug、info、warn、error级别日志
//...表示可变参数，##__VA_ARGS__表示将可变参数展开
//加上##是为了防止可变参数为空时，编译器报错
#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);  // debug级别日志
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);  // info级别日志
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);  // warn级别日志
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);  // error级别日志

#endif // LOG_H