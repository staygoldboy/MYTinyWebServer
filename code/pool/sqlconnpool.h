#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"

class SqlConnPool{
public:
    static SqlConnPool* Instance();

    MYSQL* GetConn();
    void FreeConn(MYSQL* conn);

    int GetFreeConnCnt();
    void ClosePool();

    void Init(const char* host, uint16_t port, const char* user, const char* passwd, const char* db_name, int maxConn);

private:
    SqlConnPool() = default;
    ~SqlConnPool() {ClosePool();}

    int MAX_CONN_;

    std::queue<MYSQL*> connQue_;  // 连接队列
    std::mutex mtx_;
    sem_t semId_;  //信号量
};

class SqlConnRAII{  
public:
    SqlConnRAII(SqlConnPool* sqlconn, MYSQL** sql){
        assert(sqlconn);
        *sql = sqlconn->GetConn();
        sql_ = *sql;
        sqlconn_ = sqlconn;
    }

    ~SqlConnRAII(){
        if(sql_){
            sqlconn_->FreeConn(sql_);
        }
    }


private:
    MYSQL* sql_;
    SqlConnPool* sqlconn_;

};


#endif /* SQLCONNPOOL_H */