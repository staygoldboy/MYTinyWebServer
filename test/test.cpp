#include "../code/log/log.h"
#include <features.h>


#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
#include <sys/syscall.h>
#include <unistd.h>
#define gettid() syscall(SYS_gettid)
#endif

void TestLog(){
    int cnt = 0;
    int level = 0;
    Log::Instance()->init(level, "./testlog1", ".log", 0);
    for(level = 3; level >= 0; level--)
    {
        Log::Instance()->SetLevel(level);
        for(int j = 0; j < 10000; j++)
        {
            for(int i = 0; i < 4; i++)
            {
                LOG_BASE(i, "%s 1111111 %d ========", "hello", cnt++);
            }
        }
    }
    cnt = 0;
    Log::Instance()->init(level, "./testlog2", ".log", 5000);
    for(level = 0; level < 4; level++)
    {
        Log::Instance()->SetLevel(level);
        for(int j = 0; j < 10000; j++)
        {
            for(int i = 0; i < 4; i++)
            {
                LOG_BASE(i, "%s 2222222 %d ========", "hello", cnt++);
            }
        }
    }
}


int main(){
    TestLog();
}