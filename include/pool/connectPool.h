#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <iostream>
#include <string>
#include <mysql/mysql.h>
#include <hiredis/hiredis.h>

/*
该连接池负责两种连接：
MYSQL和Redis
*/

template <typename T>
class ConnectionPool{
public:
    std::shared_ptr<T> getConnection();
    void returnConnection(std::shared_ptr<T> conn);
    void initPool(size_t poolSize);

protected:
    std::queue<std::shared_ptr<T>> pool;
    std::mutex poolMutex;
    std::condition_variable cond;
    size_t maxPoolSize = 10;

    virtual std::shared_ptr<T> createConnection() = 0;
};

// MYSQL连接池
class MySQLConnectionPool: public ConnectionPool<MYSQL> {
public:
    MySQLConnectionPool(const std::string& host, const std::string& user, const std::string& dbname, unsigned int port);
    ~MySQLConnectionPool();

private:
    std::string host;
    std::string user;
    std::string password;
    std::string dbname;
    unsigned int port;

    // 初始化MYSQL连接
    std::shared_ptr<MYSQL> createConnection() override;
};

// Redis连接池
class RedisConnectionPool: public ConnectionPool<redisContext> {
public:
    RedisConnectionPool(const std::string& host, int port);
    ~RedisConnectionPool();

private:
    std::string host;
    int port;

    // 初始化 redis 连接
    std::shared_ptr<redisContext> createConnection() override;
};


template <typename T>
void ConnectionPool<T>::initPool(size_t poolSize)
{
    std::unique_lock<std::mutex> lock(poolMutex);
    for (size_t i = 0; i < poolSize; ++ i) {
        auto conn = createConnection();
        if (conn) {
            pool.push(conn);
        } else {
            // ... 引入日志
        }
    }
}

template <typename T>
std::shared_ptr<T> ConnectionPool<T>::getConnection()
{
    std::unique_lock<std::mutex> lock(poolMutex);
    cond.wait(lock, [this](){return !pool.empty();});

    auto conn = pool.front();
    pool.pop();
    return conn;
}

template <typename T>
void ConnectionPool<T>::returnConnection(std::shared_ptr<T> conn)
{
    std::unique_lock<std::mutex> lock(poolMutex);
    pool.push(conn);
    cond.notify_one();
}

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
    : host(host), port(port) {
    initPool(maxPoolSize);
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