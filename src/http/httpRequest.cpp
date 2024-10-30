#include "http/httpRequest.h"
#include "util/util.h"

using namespace std;

// 在解析错误的时候定位bug
std::string checkData;

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

void HttpRequest::Init() {
    state_ = REQUEST_LINE;
    method_ = path_ = version_= body_ = "";
    header_.clear();
    post_.clear();
}

bool HttpRequest::parse(LinearBuffer& buff) {
    std::string END = "\r\n";
    if(buff.readAbleBytes() == 0)
        return false;
    checkData = buff.justGetData();

    while(buff.readAbleBytes() && state_ != FINISH) {
        int *Errno = 0;
        string line;
        // 请求体的后面部分并没有结束符号
        if (state_ == BODY) {
            line  = buff.getDataByLength(stoi(header_["Content-Length"]));
        } else {
            line = buff.getByEndFlag(END);
        }
        
        switch (state_)
        {
        case REQUEST_LINE:
            if(!ParseRequestLine_(line)) {
                return false;
            }
            ParsePath_();
            break;
        case HEADERS:
            ParseHeader_(line);
            if(buff.readAbleBytes() <= 2) {
                state_ = FINISH;
                buff.getReadAbleBytes();
            }
            break;
        case BODY:
            ParseBody_(line);
            break;
        default:
            break;
        }

    }
    checkData = "";
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

bool HttpRequest::ParseRequestLine_(const string& line) {
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch Match;
    if(regex_match(line, Match, patten)) {
        method_ = Match[1];
        path_ = Match[2];
        version_ = Match[3];
        state_ = HEADERS;
        return true;
    }
    LOG_ERROR("RequestLine Error, Error meesage is: %s", checkData);
    return false;
}

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
    } else {
        state_ = BODY;
    }
}

void HttpRequest::ParseBody_(const string& line) {
    body_ = line;
    ParsePost_();
    state_ = FINISH;
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

void HttpRequest::ParsePost_() {
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded_();
        if(DEFAULT_HTML_TAG.count(path_)) {
            int tag = DEFAULT_HTML_TAG.find(path_)->second; 
            LOG_DEBUG("Tag:%d", tag);
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);
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
    if(body_.size() == 0) { return; }

    string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for(; i < n; i++) {
        char ch = body_[i];
        switch (ch) {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            body_[i] = ' ';
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
    if(post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

// 加入redis缓存
// 为了保证数据的一致性，对于读数据，使用redis + mysql，而对于写数据，只使用mysql
bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin) 
{
    if (name.empty() || pwd.empty()) return false;
    auto redisConn = m_redis->getConnection();
    if (!redisConn) {
        LOG_ERROR("Redis connection error!");
        return false;
    }
    
    LOG_INFO("About database, is login:%s", isLogin ? "true" : "false");

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

int HttpRequest::ConverHex(char ch) 
{
    if(ch >= 'A' && ch <= 'F') 
        return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') 
        return ch -'a' + 10;
    return ch;
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