#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <iostream>
#include <string>
#include <mysql/mysql.h>
#include <hiredis/hiredis.h>


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