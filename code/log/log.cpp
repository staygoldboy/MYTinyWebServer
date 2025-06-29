#include "log.h"

using namespace std;

Log::Log(){
    fp_ = nullptr;
    deque_ = nullptr;
    writeThread_ = nullptr;
    lineCount_ = 0;
    toDay_ = 0;
    isAsync_ = false;
}

Log::~Log(){
    while(!deque_->empty()){
        deque_->flush();  //唤醒消费者，处理完队列中的所有任务
    }
    deque_->close();  //关闭队列
    writeThread_->join(); //等待当前线程执行完
    if(fp_){               //如果文件指针不为空，则关闭文件
        lock_guard<mutex> locker(mtx_); 
        Flush();     //清空缓冲区的数据
        fclose(fp_);  //关闭日志文件
    }
}

void Log::Flush(){
    if(isAsync_){     //如果为异步写日志，则唤醒消费者线程
        deque_->flush();
    }
    fflush(fp_);   //清空缓冲区的数据
}

//懒汉模式 局部静态变量法 不需要加锁
//返回一个指向Log对象的指针
Log* Log::Instance(){
    static Log log;
    return &log;
}

//异步日志的写线程函数
void Log::FlushLogThread(){
    Log::Instance()->AsynWriteLog();
}

//异步写日志
void Log::AsynWriteLog(){
    string str = "";
    while(deque_->pop(str)){
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

void Log::init(int level, const char* path, const char* suffix, int maxQueueCapacity){
    isOpen_ = true;
    level_ = level;
    path_ = path;
    suffix_ = suffix;
    if(maxQueueCapacity > 0){
        isAsync_ = true;
        if(!deque_)
        {
            unique_ptr<BlockQueue<string>> newQue(new BlockQueue<string>); //unique_ptr智能指针，自动释放内存
            deque_ = move(newQue);  //移动语义，将newQue指针赋值给deque_，newQue指针置空

            unique_ptr<thread> newThread(new thread(FlushLogThread)); //unique_ptr智能指针，自动释放内存
            writeThread_ = move(newThread); //移动语义，将newThread指针赋值给writeThread_，newThread指针置空
        }
    }
    else{
        isAsync_ = false;
    }

    lineCount_ = 0;

    time_t timer = time(nullptr);     //获取当前时间
    struct tm *sysTime = localtime(&timer);   //获取本地时间
    struct tm logTime = *sysTime;   //获取本地时间
    char fileName[LOG_NAME_LEN] = {0};    //日志文件名
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d%02d%02d%s", path_, logTime.tm_year + 1900, logTime.tm_mon + 1, logTime.tm_mday, suffix_);
    toDay_ = logTime.tm_mday;

    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll();
        if(fp_){     //重新打开文件
            Flush();
            fclose(fp_);
        }
        fp_ = fopen(fileName, "a");  //以追加的方式打开文件
        if(fp_ == nullptr){
            mkdir(path_,0777);    //创建目录,0777表示权限,即所有用户可读可写可执行
            fp_ = fopen(fileName, "a"); 
        }
        assert(fp_ != nullptr);  //断言，如果fp_为空，则程序终止
    }

}

void Log::WriteLog(int level, const char* format, ...)
{
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);   //获取当前时间
    time_t tSec = now.tv_sec;   //
    struct tm *sysTime = localtime(&tSec);   //获取本地时间
    struct tm logTime = *sysTime; 
    va_list vaList;

    if(toDay_ != logTime.tm_mday || (lineCount_ && (lineCount_ % MAX_LINES == 0))){
        unique_lock<mutex> locker(mtx_);
        locker.unlock();

        char newFileName[LOG_NAME_LEN] = {0};
        char tail[36] = {0};  
        snprintf(tail, 36, "%04d_%02d_%02d", logTime.tm_year + 1900, logTime.tm_mon + 1, logTime.tm_mday);
        if(toDay_ != logTime.tm_mday){
            snprintf(newFileName, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);  //
            toDay_ = logTime.tm_mday;
            lineCount_ = 0;  //如果日期改变，则将行数置为0
        }
        else{
            snprintf(newFileName, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_ / MAX_LINES), suffix_);  
        }
        locker.lock();
        Flush();
        fclose(fp_);
        fp_ = fopen(newFileName, "a");
        assert(fp_ != nullptr);
    }


    //在buff内生成一条对应的日志信息
    {
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        int n = snprintf(buff_.BeginWrite(),128, "%04d-%02d-%02d %02d:%02d:%02d.%06ld ",
            logTime.tm_year + 1900, logTime.tm_mon + 1, logTime.tm_mday,
            logTime.tm_hour, logTime.tm_min, logTime.tm_sec, now.tv_usec);
        assert(n > 0);
        buff_.HasWritten(n);
        AppendLogLevelTitle(level);

        va_start(vaList, format);
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);  //将可变参数格式化到buff_中
        va_end(vaList);

        buff_.HasWritten(m);
        buff_.Append("\n\0", 2);

        if(isAsync_ && deque_ && !deque_->full()){  //如果为异步写日志，则将日志信息放入队列中
            deque_->push_back(buff_.RetrieveAllToStr());  //将buff_中的数据放入队列中
        }
        else{
            fputs(buff_.Peek(), fp_);  //将buff_中的数据写入文件
        }
        buff_.RetrieveAll(); //清空buff_
    }
}

//根据日志级别，添加对应的日志级别标题
void Log::AppendLogLevelTitle(int level){
    switch(level){
        case 0:
            buff_.Append("[debug]: ", 9);
            break;
        case 1:
            buff_.Append("[info]: ", 9);
            break;
        case 2:
            buff_.Append("[warn]: ", 9);
            break;
        case 3:
            buff_.Append("[error]: ", 9);
            break;
        default:
            buff_.Append("[info]: ", 9);
            break;
    }
}


//获取日志级别
int Log::GetLevel(){
    lock_guard<mutex> locker(mtx_);
    return level_;
}

void Log::SetLevel(int level){
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}
