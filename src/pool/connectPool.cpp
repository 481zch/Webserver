#include "pool/connectPool.h"

// MYSQL连接池的继承实现
MySQLConnectionPool::MySQLConnectionPool(const std::string &host, const std::string &user, 
            const std::string& password, const std::string &dbname, unsigned int port):
            host(host), user(user), password(password), dbname(dbname), port(port)
{
    initPool(maxPoolSize);
}

MySQLConnectionPool::~MySQLConnectionPool()
{
    while (!pool.empty()) {
        auto conn = pool.front();
        pool.pop();
        mysql_close(conn);
    }
}

inline MYSQL* MySQLConnectionPool::createConnection()
{
    MYSQL* conn = mysql_init(nullptr);
    if (conn == nullptr) {
        LOG_ERROR("Mysql connection create error in init")
        return nullptr;
    }

    if (mysql_real_connect(conn, host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), 
                            port, nullptr, 0) == nullptr) 
    {
        LOG_ERROR("Mysql connection create error in connect")
        mysql_close(conn);
        return nullptr;
    }

    return conn;
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
        redisFree(conn);
    }
}

redisContext* RedisConnectionPool::createConnection()
{
    redisContext* conn = redisConnect(host.c_str(), port);
    if (conn == nullptr || conn->err) {
        if (conn) {
            LOG_ERROR("Redis connection error: %s", conn->errstr);
            redisFree(conn);
        } else {
            LOG_ERROR("Redis connection create error.");
        }
        return nullptr;
    }

    return conn;
}