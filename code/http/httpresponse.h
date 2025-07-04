#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <unordered_map>
#include <fcntl.h>    // 该头文件定义了文件控制相关的函数
#include <unistd.h>    // 该头文件定义了标准输入输出相关的函数
#include <sys/stat.h> // 该头文件定义了文件状态相关的函数
#include <sys/mman.h> // 该头文件定义了内存映射相关的函数

#include "../buffer/buffer.h"
#include "../log/log.h"

using namespace std;

class HttpResponse{
public:
    HttpResponse();
    ~HttpResponse();

    void Init(const string& srcDir, string& path, bool isKeepAlive = false, int code = -1);
    void MakeResponse(Buffer* buffer);
    void UnmapFile();
    char* File();
    size_t FileLen() const;
    void ErrorContent(Buffer& buffer, string message);
    int Code() const { return code_; };


private:
    void AddStateLine_(Buffer& buffer);
    void AddHeader_(Buffer& buffer);
    void AddContent_(Buffer& buffer);

    void ErrorHtml_();
    string GetFileType_();

    int code_;     // 状态码
    bool isKeepAlive_;    // 是否保持连接

    string path_;    // 文件路径
    string srcDir_;   // 资源文件所在目录

    char* mmFile_;    // 内存映射文件
    struct stat mmFileStat_;    // 文件状态

    static const unordered_map<string, string> SUFFIX_TYPE;    // 后缀名与文件类型的映射
    static const unordered_map<int, string> CODE_STATUS;    // 状态码与状态描述的映射
    static const unordered_map<int, string> CODE_PATH;    // 状态码与错误页面路径的映射

};

#endif  // HTTP_RESPONSE_H