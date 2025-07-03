#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <unordered_map>
#include <string>
#include <unordered_set>
#include <regex>
#include <mysql/mysql.h>
#include <errno.h>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"


using namespace std;

class HttpRequest{
public:
    enum PARSE_STATE{
        REQUEST_LINE,     // 解析请求行状态
        HEADERS,         // 解析请求头状态  
        BODY,            // 解析请求体状态
        FINISH           // 解析完成状态    
    };
    
    HttpRequest() {Init();}
    ~HttpRequest() = default;

    void Init();
    bool parse(Buffer& buff);  // 解析http请求

    string path() const;  // 获取请求路径
    string& path();      //
    string method() const;  // 获取请求方法
    string version() const;   // 获取http版本
    string GetPost(const string& key) const;   // 获取post请求参数
    string GetPost(const char* key) const; 
    
    bool IsKeepAlive() const;  // 是否保持连接
    
private:
    bool ParseRequestLine(const string& line);     // 解析请求行
    void ParseHeader(const string& line);         // 解析请求头
    void ParseBody(const string& line);            // 解析请求体

    void ParsePath();    // 解析请求路径
    void ParsePost();            // 处理post事件
    void ParseFromUrlencoded();   // 解析url编码

    static bool UserVerify(const string& username, const string& password, bool isLogin);   // 验证用户名密码

    PARSE_STATE state_;  // 解析状态
    string method_, path_, version_, body_;
    unordered_map<string, string> header_;  // 请求头
    unordered_map<string, string> post_;

    static const unordered_set<string> DEFAULT_HTML;  // 默认html文件
    static const unordered_map<string, int> DEFAULT_HTML_TAG;  // 默认html文件后缀
    static int ConverHex(char ch);  // 将16进制字符转换为10进制数字
};

#endif // HTTP_REQUEST_H