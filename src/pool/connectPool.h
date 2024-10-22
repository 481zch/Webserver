#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <iostream>
#include <string>
#include <mysql/mysql.h>
#include <hiredis/hiredis.h>
#include "log/log.h"


template <typename T>
class ConnectionPool{
public:
    T* getConnection();
    void returnConnection(T* conn);
    void initPool(size_t poolSize);
    size_t getCurNum();
    
protected:
    std::queue<T*> pool;
    std::mutex poolMutex;
    std::condition_variable cond;
    // 现在把连接池大小写死了，后期去修改为动态
    size_t maxPoolSize = 10;
    virtual T* createConnection() = 0;
};

// MYSQL连接池
class MySQLConnectionPool: public ConnectionPool<MYSQL> {
public:
    MySQLConnectionPool(const std::string& host, const std::string& user, const std::string& password, const std::string& dbname, unsigned int port);
    ~MySQLConnectionPool();

private:
    std::string host;
    std::string user;
    std::string password;
    std::string dbname;
    unsigned int port;

    // 初始化MYSQL连接
    MYSQL* createConnection() override;
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
    redisContext* createConnection() override;
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
            LOG_ERROR("Create %s connect error.", typeid(T).name());
        }
    }
}

template <typename T>
inline size_t ConnectionPool<T>::getCurNum()
{
    std::unique_lock<std::mutex> lock(poolMutex);
    return pool.size();
}

template <typename T>
T* ConnectionPool<T>::getConnection()
{
    std::unique_lock<std::mutex> lock(poolMutex);
    cond.wait(lock, [this](){return !pool.empty();});

    auto conn = pool.front();
    pool.pop();
    return conn;
}

template <typename T>
void ConnectionPool<T>::returnConnection(T* conn)
{
    std::unique_lock<std::mutex> lock(poolMutex);
    pool.push(conn);
    cond.notify_one();
}