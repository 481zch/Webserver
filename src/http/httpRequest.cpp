#include "http/httpRequest.h"
#include "util/util.h"

using namespace std;

// 网页名称，和一般的前端跳转不同，这里需要将请求信息放到后端来验证一遍再上传（和小组成员还起过争执）
const unordered_set<string> HttpRequest::DEFAULT_HTML {
    "/index", "/register", "/login", "/welcome", "/video", "/picture",
};

// 登录/注册
const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {
    {"/login.html", 1}, {"/register.html", 0}
};

HttpRequest::HttpRequest(MySQLConnectionPool* mysql, RedisConnectionPool* redis)
{
    assert(mysql != nullptr);
    assert(redis != nullptr);
    m_mysql = mysql;
    m_redis = redis;
}

// 初始化操作，一些清零操作
void HttpRequest::Init() {
    state_ = REQUEST_LINE;  // 初始状态
    method_ = path_ = version_= body_ = "";
    header_.clear();
    post_.clear();
}

// 解析处理
bool HttpRequest::parse(CircleBuffer& buff) {
    const char END[] = "\r\n";
    if(buff.readableBytes() == 0)   // 没有可读的字节
        return false;
    // 读取数据开始
    while(buff.readableBytes() && state_ != FINISH) {
        int *Errno = 0;
        string line = buff.getByEndBytes();
        switch (state_)
        {
        case REQUEST_LINE:
            // 解析错误
            if(!ParseRequestLine_(line)) {
                return false;
            }
            ParsePath_();   // 解析路径
            break;
        case HEADERS:
            ParseHeader_(line);
            // 下面的if是说明没有剩余的可读数据了
            if(buff.readableBytes() <= 2) {  // 说明是get请求，后面为\r\n
                state_ = FINISH;   // 提前结束
            }
            break;
        case BODY:
            ParseBody_(line);
            break;
        default:
            break;
        }
        // if(lineend == buff.BeginWrite()) {  // 读完了
        //     buff.RetrieveAll();
        //     break;
        // }
        // buff.RetrieveUntil(lineend + 2);        // 跳过回车换行

    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

bool HttpRequest::ParseRequestLine_(const string& line) {
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch Match;   // 用来匹配patten得到结果
    // 在匹配规则中，以括号()的方式来划分组别 一共三个括号 [0]表示整体
    if(regex_match(line, Match, patten)) {  // 匹配指定字符串整体是否符合
        method_ = Match[1];
        path_ = Match[2];
        version_ = Match[3];
        state_ = HEADERS;
        return true;
    }
    LOG_ERROR("RequestLine Error, line is: %s", line);
    return false;
}

// 解析路径，统一一下path名称,方便后面解析资源
void HttpRequest::ParsePath_() {
    if(path_ == "/") {
        path_ = "/index.html";
    } else {
        if(DEFAULT_HTML.find(path_) != DEFAULT_HTML.end()) {
            path_ += ".html";
        }
    }
}

void HttpRequest::ParseHeader_(const string& line) {
    regex patten("^([^:]*): ?(.*)$");
    smatch Match;
    if(regex_match(line, Match, patten)) {
        header_[Match[1]] = Match[2];
    } else {    // 匹配失败说明首部行匹配完了，状态变化
        state_ = BODY;
    }
}

void HttpRequest::ParseBody_(const string& line) {
    body_ = line;
    ParsePost_();
    state_ = FINISH;    // 状态转换为下一个状态
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

// 处理post请求
void HttpRequest::ParsePost_() {
    // 用于表单提交的格式
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded_();     // POST请求体示例
        if(DEFAULT_HTML_TAG.count(path_)) { // 如果是登录/注册的path
            int tag = DEFAULT_HTML_TAG.find(path_)->second; 
            LOG_DEBUG("Tag:%d", tag);
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);  // 为1则是登录
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    path_ = "/welcome.html";
                } 
                else {
                    path_ = "/error.html";
                }
            }
        }
    }   
}

// 从url中解析编码
// 这样写也很不严谨，对于特殊符号编码的后期数据库查询有一些问题
void HttpRequest::ParseFromUrlencoded_() {
    auto [first, second] = split(body_, '&');
    auto [a, username] = split(first, '=');
    auto [b, password] = split(second, '=');
    post_[username] = password;
}

// 需要重写，加入Redis缓存
// 为了保证数据的一致性，对于读数据，使用redis + mysql，而对于写数据，只使用mysql
bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin) 
{
    if (name.empty() || pwd.empty()) return false;
    auto redisConn = m_redis->getConnection();
    if (!redisConn) {
        LOG_ERROR("Redis connection error!");
        return false;
    }

    std::string redisCmd = "GET " + name;
    redisReply* reply = (redisReply*)redisCommand(redisConn, redisCmd.c_str());
    if (reply && reply->type == REDIS_REPLY_STRING) {
        std::string cachedPwd = reply->str;
        freeReplyObject(reply);

        if (isLogin) {
            if (pwd == cachedPwd) {
                LOG_INFO("User %s logged in with cache", name.c_str());
                return true;
            } else {
                LOG_INFO("User %s login failed with cache (wrong password)", name.c_str());
                return false;
            }
        } else {
            LOG_INFO("User %s already exists (checked with cache)", name.c_str());
            return false;
        }
    }

    // Redis 缓存没有命中，转 MySQL 查询
    auto mysqlConn = m_mysql->getConnection();
    if (!mysqlConn) {
        LOG_ERROR("MySQL connection error!");
        return false;
    }

    char sqlQuery[256];
    snprintf(sqlQuery, 256, "SELECT password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", sqlQuery);

    if (mysql_query(mysqlConn, sqlQuery)) {
        LOG_ERROR("MySQL query error!");
        return false;
    }

    MYSQL_RES* res = mysql_store_result(mysqlConn);
    if (res == nullptr) {
        LOG_ERROR("MySQL store result error!");
        return false;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    bool success = false;

    if (row != nullptr) {
        std::string dbPwd(row[0]);

        if (isLogin) {
            if (pwd == dbPwd) {
                success = true;
                LOG_INFO("User %s logged in with MySQL", name.c_str());
            } else {
                LOG_INFO("User %s login failed with MySQL (wrong password)", name.c_str());
            }
        } else {
            LOG_INFO("User %s already exists (checked with MySQL)", name.c_str());
            success = false;
        }

        // 将用户数据写入 Redis 缓存
        redisCmd = "SET " + name + " " + dbPwd;
        reply = (redisReply*)redisCommand(redisConn, redisCmd.c_str());
        if (reply) freeReplyObject(reply);
    } else if (!isLogin) {
        // 注册新用户
        snprintf(sqlQuery, 256, "INSERT INTO user(username, password) VALUES('%s', '%s')", name.c_str(), pwd.c_str());
        if (mysql_query(mysqlConn, sqlQuery) == 0) {
            success = true;
            LOG_INFO("User %s registered successfully!", name.c_str());
        } else {
            LOG_ERROR("MySQL insert error!");
        }
    }

    mysql_free_result(res);
    return success;

}

std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}

std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}


// 查找post请求中的键值的
std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

// 来检查是否要求保持连接（长连接）
bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}