#include "pool/connectPool.h"

// MYSQL连接池的继承实现
MySQLConnectionPool::MySQLConnectionPool(const std::string &host, const std::string &user, 
            const std::string &dbname, unsigned int port):
            host(host), user(user), password(password), dbname(dbname), port(port)
{
    initPool(maxPoolSize);
}

MySQLConnectionPool::~MySQLConnectionPool()
{
    while (!pool.empty()) {
        auto conn = pool.front();
        pool.pop();
        mysql_close(conn.get());
    }
}

inline std::shared_ptr<MYSQL> MySQLConnectionPool::createConnection()
{
    MYSQL* conn = mysql_init(nullptr);
    if (conn == nullptr) {
        // 引入日志
        return nullptr;
    }

    if (mysql_real_connect(conn, host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), 
                            port, nullptr, 0) == nullptr) 
    {
        // 引入日志
        mysql_close(conn);
        return nullptr;
    }

    return std::shared_ptr<MYSQL>(conn, mysql_close);
}

// Redis连接池的实现        
RedisConnectionPool::RedisConnectionPool(const std::string& host, int port)
                                        :host(host), port(port) 
{
    initPool(maxPoolSize);
}

RedisConnectionPool::~RedisConnectionPool()
{
    while (!pool.empty()) {
        auto conn = pool.front();
        pool.pop();
        redisFree(conn.get());
    }
}

std::shared_ptr<redisContext> RedisConnectionPool::createConnection()
{
    redisContext* conn = redisConnect(host.c_str(), port);
    if (conn == nullptr || conn->err) {
        if (conn) {
            // 引入日志
            redisFree(conn);
        } else {
            // 引入日志
        }
        return nullptr;
    }

    return std::shared_ptr<redisContext>(conn, redisFree);
}