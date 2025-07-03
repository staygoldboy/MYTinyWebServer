#include "httprequest.h"

using namespace std;


//网页名称，和一般的前端跳转不同，这里需要将请求信息封装成http请求，发送给服务器，再上传给前端
const unordered_set<string> HttpRequest::DEFAULT_HTML{
    "/index", "/register", "/login", "/welcome", "/video", "/picture"
};

//登录注册页面
const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG{
    {"/register.html", 0}, {"/login.html", 1}
};


//初始化
void HttpRequest::Init() {
    state_ = REQUEST_LINE;
    method_ = path_ = version_ = body_ = "";
    header_.clear();
    post_.clear();
}

//解析http请求
bool HttpRequest::parse(Buffer& buff){
    const char END[] = "\r\n";
    if(buff.ReadableBytes() == 0)  //如果缓冲区为空，则返回false
    {
        return false;
    }
    while(buff.ReadableBytes() && state_ != FINISH)
    {
        // 从buff中的读指针开始到读指针结束，这块区域是未读取得数据并去处"\r\n"，返回有效数据得行末指针
        const char* line_end = search(buff.Peek(), buff.BeginWritePtr(), END, END + 2);  //
        string line(buff.Peek(), line_end);
        switch (state_)
        {
        case REQUEST_LINE:
            if(!ParseRequestLine(line)){
                return false;
            }
            ParsePath();    //解析路径
            break;
        case HEADERS:
            ParseHeader(line);
            if(buff.ReadableBytes() <= 2)     //如果缓冲区中只剩下"\r\n"，则说明是get请求，直接结束，get可以没有请求体
            {
                state_ = FINISH;
            }
            break;
        case BODY:
            ParseBody(line);
        default:
            break;
        }
        if(line_end == buff.BeginWritePtr())  //如果line_end == buff.BeginWritePtr()，说明没有找到"\r\n"，则读完了
        {
            buff.RetrieveAll();
            break;
        }
        buff.RetrieveUntil(line_end + 2);  //跳过回车换行
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

//解析请求行
bool HttpRequest::ParseRequestLine(const string& line) {
    regex pattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");  //在匹配规则中，以括号括起来的部分，就是匹配结果
    smatch Match;    //匹配结果
    if(regex_match(line, Match, pattern))  //匹配成功
    {
        method_ = Match[1];
        path_ = Match[2];
        version_ = Match[3];
        state_ = HEADERS;
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}


//解析路径
void HttpRequest::ParsePath() {
    if(path_ == "/")  //如果路径为空，则默认为index.html
    {
        path_ = "/index.html";
    }
    else if(DEFAULT_HTML.count(path_))  //如果路径在默认网页中，则加上.html后缀
    {
        path_ += ".html";
    }
}



//解析请求头
void HttpRequest::ParseHeader(const string& line) {
    regex pattern("^([^:]*): ?(.*)$");
    smatch Match;
    if(regex_match(line, Match, pattern))
    {
        header_[Match[1]] = Match[2];
    }
    else    //如果匹配失败，说明已经读到请求体了
    {
        state_ = BODY;
    }
}


//解析请求体
void HttpRequest::ParseBody(const string& line) {
    body_ = line;
    ParsePost();
    state_ = FINISH;
    LOG_DEBUG("Body: %s, len = %d", body_.c_str(), body_.size());
}

//解析post请求体
void HttpRequest::ParsePost() {
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded")
    {
        ParseFromUrlencoded();  //Post请求，且请求头中Content-Type为application/x-www-form-urlencoded，则解析请求体
        if(DEFAULT_HTML_TAG.count(path_))  //如果请求路径在为登录/注册
        {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            if(tag == 0 || tag == 1)
            {
                bool isLogin = (tag == 1);
                if(UserVerify(post_["username"], post_["password"], isLogin))
                {
                    path_ = "/welcome.html";
                }
                else
                {
                    path_ = "/error.html";
                }
            }
        }
    }
}

//16进制转10进制
int HttpRequest::ConverHex(char ch) {
    if(ch >= 'a' && ch <= 'f')
    {
        return ch - 'a' + 10;
    }
    if(ch >= 'A' && ch <= 'F')
    {
        return ch - 'A' + 10;
    }
    return ch;
}


//解析url编码
void HttpRequest::ParseFromUrlencoded() {
    if(body_.size() == 0)
    {
        return;
    }
    string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;
    for(; i < n; i++)
    {
        char ch = body_[i];
        switch(ch)    //根据不同的字符进行不同的处理
        {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            body_[i] = ' ';  //将+替换为空格
            break;
        case '%':
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            body_[i + 2] = num % 10 + '0';
            body_[i + 1] = num / 10 + '0';
            i += 2;
            break;
        case '&':
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    assert(j <= i);
    if(post_.count(key) == 0 && j < i)
    {
        value = body_.substr(j, i - j);
        post_[key] = value;
        LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
    }
}

// 用户验证
bool HttpRequest::UserVerify(const string& name, const string& pwd, bool isLogin) {
    if(name == "" || pwd == "")
    {
        return false;
    }
    LOG_INFO("Verify name = %s, pwd = %s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    SqlConnRAII(SqlConnPool::Instance(),&sql);
    assert(sql);

    bool flag = false;
    char order[256] = {0};
    MYSQL_FIELD* fields = nullptr;  //字段
    MYSQL_RES* res_ptr = nullptr;   //结果集

    if(!isLogin) flag = true;  //注册
    //查询用户名是否存在
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());   //查询语句的含义是：在user表中，查询username为name的用户，且只返回一条记录
    LOG_DEBUG("%s", order);

    if(mysql_query(sql, order))  //执行查询语句，如果失败，则返回非零值
    {
        mysql_free_result(res_ptr);  //释放结果集
        LOG_ERROR("Query Error: %s", mysql_error(sql));
        return false;
    }
    res_ptr = mysql_store_result(sql);  //将查询结果存储到结果集中
    fields = mysql_fetch_fields(res_ptr);  //获取结果集中的字段

    while(MYSQL_ROW row = mysql_fetch_row(res_ptr))  //遍历结果集中的每一行
    {
        LOG_DEBUG("username = %s, password = %s", row[0], row[1]);
        string password(row[1]);
        //登录行为，验证密码是否正确
        if(isLogin)
        {
            if(pwd == password)
            {
                flag = true;
            }
            else
            {
                flag = false;
                LOG_INFO("Password is not correct");
            }
        }

    }
    mysql_free_result(res_ptr);  //释放结果集

    if(!isLogin && flag == true)  //注册行为，且用户名不存在,将用户名和密码插入到数据库中
    {
        LOG_DEBUG("Regirster!");
        bzero(order, 256);
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s', '%s')", name.c_str(), pwd.c_str());
        if(mysql_query(sql, order))
        {
            LOG_ERROR("Insert Error: %s", mysql_error(sql));
            return false;
        }
        else
        {
            flag = true;
        }
    }
    LOG_INFO("UserVerify success");
    return flag;
}

string HttpRequest::path() const{
    return path_;
}

string& HttpRequest::path(){
    return path_;
}

string HttpRequest::method() const{
    return method_;
}

string HttpRequest::version() const{
    return version_;
}

string HttpRequest::GetPost(const string& key) const{
    assert(key != "");
    if(post_.count(key) == 1)
    {
        return post_.find(key)->second;
    }
    return "";
}

string HttpRequest::GetPost(const char* key) const{
    assert(key != nullptr);
    if(post_.count(key) == 1)
    {
        return post_.find(key)->second;
    }
    return "";
}

bool HttpRequest::IsKeepAlive() const{
    if(header_.count("Connection") == 1)
    {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}


