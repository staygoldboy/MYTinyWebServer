#include "httpresponse.h"

using namespace std;

const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};       // 文件后缀与文件类型映射


const unordered_map<int, string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};      // 响应状态码与状态描述映射


const unordered_map<int, string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};      // 响应状态码与错误页面映射

HttpResponse::HttpResponse() {
    code_ = -1;
    path_ = nullptr;
    srcDir_ = nullptr;
    isKeepAlive_ = false;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
}

HttpResponse::~HttpResponse() {
    UnmapFile();
}


// 初始化响应对象
void HttpResponse::Init(const string& srcDir, string& path, bool isKeepAlive, int code) {
    assert(srcDir != "");
    if(mmFile_) 
    {
        UnmapFile();
    }
    code_ = code;
    path_ = path;
    srcDir_ = srcDir;
    isKeepAlive_ = isKeepAlive;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
}

// 生成响应报文
void HttpResponse::MakeResponse(Buffer& buffer){
    if(stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {  //文件不存在或为目录，stat函数获取文件信息，如果文件不存在返回-1，S_ISDIR判断是否为目录
        code_ = 404;
    }
    else if(!(mmFileStat_.st_mode & S_IROTH)) {  //文件不可读
        code_ = 403;
    }
    else if(code_ == -1){
        code_ = 200;
    }

    ErrorHtml_();   //错误页面
    AddStateLine_(buffer);  // 状态行
    AddHeader_(buffer);  // 响应头
    AddContent_(buffer);   // 响应体
}


char* HttpResponse::File() {
    return mmFile_;
}

size_t HttpResponse::FileLen() const{
    return mmFileStat_.st_size;
}


// 解析错误页面
void HttpResponse::ErrorHtml_() {
    if(CODE_PATH.count(code_) == 1){
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);   //获取文件信息
    }
}


// 添加状态行
void HttpResponse::AddStateLine_(Buffer& buffer) {
    string status;
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    }
    else {
        code_ = 400;
        status = CODE_STATUS.find(code_)->second;
    }
    buffer.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");
}


// 添加响应头
void HttpResponse::AddHeader_(Buffer& buffer) {
    buffer.Append("Connection: ");
    if(isKeepAlive_)
    {
        buffer.Append("keep-alive\r\n");
        buffer.Append("keep-alive: max=6, timeout=120\r\n");
    }
    else
    {
        buffer.Append("close\r\n");
    }
    buffer.Append("Content-type: " + GetFileType_() + "\r\n");
}


// 添加响应体
void HttpResponse::AddContent_(Buffer& buffer) {
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);   // 以只读方式打开文件
    if(srcFd < 0) {
        ErrorContent(buffer, "File Not Found!!!");
        return;
    }
    
    LOG_DEBUG("file path: %s", (srcDir_ + path_).data());  
    //mmap函数将文件映射到内存中,返回指向映射区域的指针,PROT_READ表示映射区域可读,MAP_PRIVATE表示创建一个私有的映射副本
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(*mmRet == -1) {
        ErrorContent(buffer, "Internal Server Error!!!");
        return;
    }
    mmFile_ = (char*)mmRet;
    close(srcFd);
    buffer.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

// 关闭内存映射
void HttpResponse::UnmapFile() {
    if(mmFile_){
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

// 获取文件类型
string HttpResponse::GetFileType_() {
    string::size_type idx = path_.find_last_of('.');  // 找到最后一个.的位置,返回下标
    if(idx == string::npos) {    //没有找到，find_last_of返回string::npos
        return "text/plain";   //默认类型
    }
    string suffix = path_.substr(idx);  // 截取最后一个.之后的部分
    if(SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;   //返回文件类型
    }
    return "text/plain";
}


// 向Buffer中添加错误信息
void HttpResponse::ErrorContent(Buffer& buffer, string message) {
    // 定义一个字符串变量content，用于存储错误信息
    string content;
    // 定义一个字符串变量status，用于存储错误状态码
    string status;
    // 向content中添加html标签和标题
    content += "<html><title>Error</title>";  
    // 向content中添加body标签和背景颜色
    content += "<body bgcolor=\"ffffff\">";
    // 如果code_在CODE_STATUS中存在，则将对应的status赋值给status变量
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    }
    // 否则将status赋值为"Bad Request"
    else
    {
        status = "Bad Request";
    }
    // 向content中添加错误状态码和status
    content += to_string(code_) + " : " + status + "\n";
    // 向content中添加错误信息
    content += "<p>" + message + "</p>";
    // 向content中添加hr标签和服务器名称
    content += "<hr><em>MYTinyWebServer</em></body></html>";

    // 向buffer中添加content的长度
    buffer.Append("Content-length: " + to_string(content.size()) + "\r\n\r\n");
    // 向buffer中添加content
    buffer.Append(content);
}