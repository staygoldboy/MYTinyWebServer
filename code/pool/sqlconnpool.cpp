#include "sqlconnpool.h"

// 懒汉式单例模式
// 返回SqlConnPool类的实例
SqlConnPool* SqlConnPool::Instance()
{
    // 静态局部变量，只在第一次调用时初始化
    static SqlConnPool connPool;
    // 返回静态局部变量的地址
    return &connPool;
}

// 初始化连接池
void SqlConnPool::Init(const char* host, uint16_t port, const char* user, const char* passwd, const char* db_name, int maxConn = 10) 
{
    // 断言最大连接数大于0
    assert(maxConn > 0);
    // 循环创建最大连接数个连接
    for(int i = 0;i < maxConn; i++)
    {
        MYSQL* conn = nullptr;
        // 初始化连接
        conn = mysql_init(conn);
        if(!conn){
            // 如果初始化失败，输出错误日志，并断言
            LOG_ERROR("Mysql init error!");
            assert(conn);
        }
        // 连接数据库
        conn = mysql_real_connect(conn, host, user, passwd, db_name, port, nullptr, 0);
        if(!conn){
            // 如果连接失败，输出错误日志，并断言
            LOG_ERROR("Mysql connect error!");
            assert(conn);
        }
        // 输出连接成功日志
        LOG_INFO("Mysql connect success!");
        // 将连接放入连接队列
        connQue_.emplace(conn);
    }
    // 设置最大连接数
    MAX_CONN_ = maxConn;
    // 初始化信号量
    sem_init(&semId_, 0, MAX_CONN_);
}


// 获取连接
MYSQL* SqlConnPool::GetConn(){
    MYSQL* conn = nullptr;
    if(connQue_.empty()){
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&semId_);  // 等待信号量，当信号量大于0时，减1，否则阻塞
    lock_guard<mutex> locker(mtx_);
    conn = connQue_.front();
    connQue_.pop();
    return conn;
}

// 存入连接池，实际上并没有关闭连接，只是将连接放入连接队列
void SqlConnPool::FreeConn(MYSQL* conn){
    assert(conn);
    lock_guard<mutex> locker(mtx_);
    connQue_.push(conn);
    sem_post(&semId_);  // 信号量加1
}

// 关闭连接池
void SqlConnPool::ClosePool(){
    lock_guard<mutex> locker(mtx_);
    while(!connQue_.empty()){
        auto conn = connQue_.front();
        connQue_.pop();
        mysql_close(conn);
    }
    mysql_library_end();  // 关闭mysql库
}


int SqlConnPool::GetFreeConnCnt(){
    lock_guard<mutex> locker(mtx_);
    return connQue_.size();
}