#include <gtest/gtest.h>
#include <thread>
#include "pool/connectPool.h"

// 创建连接测试
TEST(createConnection, mysqlAndRedis) 
{
    MySQLConnectionPool* p_sqlPool = new MySQLConnectionPool("192.168.19.133", "root", "123456", "studentDB", 3306);
    RedisConnectionPool* p_redisPool = new RedisConnectionPool("192.168.19.133", 6379);

    MYSQL* mysqlConn = p_sqlPool->getConnection();
    redisContext* redisConn = p_redisPool->getConnection();

    ASSERT_EQ(p_sqlPool->getCurNum(), 9);
    ASSERT_EQ(p_redisPool->getCurNum(), 9);

    p_sqlPool->returnConnection(mysqlConn);
    p_redisPool->returnConnection(redisConn);

    ASSERT_EQ(p_sqlPool->getCurNum(), 10);
    ASSERT_EQ(p_redisPool->getCurNum(), 10);

    delete p_sqlPool;
    delete p_redisPool;
}

// 多线程测试
TEST(multiThreadRun, mysqlAndRedis)
{
    MySQLConnectionPool* p_sqlPool = new MySQLConnectionPool("192.168.19.133", "root", "123456", "studentDB", 3306);
    RedisConnectionPool* p_redisPool = new RedisConnectionPool("192.168.19.133", 6379);

    auto mysqlTask = [&]() {
        MYSQL* conn = p_sqlPool->getConnection();
        ASSERT_NE(conn, nullptr);  // 确保连接非空
        p_sqlPool->returnConnection(conn);
    };

    auto redisTask = [&]() {
        redisContext* conn = p_redisPool->getConnection();
        ASSERT_NE(conn, nullptr);  // 确保连接非空
        p_redisPool->returnConnection(conn);
    };

    // 创建多个线程执行任务
    std::thread t1(mysqlTask);
    std::thread t2(mysqlTask);
    std::thread t3(redisTask);
    std::thread t4(redisTask);

    // 等待线程结束
    t1.join();
    t2.join();
    t3.join();
    t4.join();

    delete p_sqlPool;
    delete p_redisPool;
}

// 数据查询测试
TEST(queryData, mysqlAndRedis)
{
    MySQLConnectionPool* p_sqlPool = new MySQLConnectionPool("192.168.19.133", "root", "123456", "studentDB", 3306);
    RedisConnectionPool* p_redisPool = new RedisConnectionPool("192.168.19.133", 6379);

    // MySQL 查询测试
    MYSQL* mysqlConn = p_sqlPool->getConnection();
    ASSERT_NE(mysqlConn, nullptr);

    const char* query = "SELECT 1";  // 执行一个简单的查询
    if (mysql_query(mysqlConn, query)) {
        FAIL() << "MySQL query failed: " << mysql_error(mysqlConn);
    } else {
        MYSQL_RES* res = mysql_store_result(mysqlConn);
        ASSERT_NE(res, nullptr);  // 确保结果集非空
        mysql_free_result(res);
    }
    p_sqlPool->returnConnection(mysqlConn);

    // Redis 查询测试
    redisContext* redisConn = p_redisPool->getConnection();
    ASSERT_NE(redisConn, nullptr);

    redisReply* reply = (redisReply*) redisCommand(redisConn, "PING");
    ASSERT_NE(reply, nullptr);  // 确保结果非空
    ASSERT_EQ(std::string(reply->str), "PONG");  // 确保 PING 的结果为 PONG
    freeReplyObject(reply);

    p_redisPool->returnConnection(redisConn);

    delete p_sqlPool;
    delete p_redisPool;
}
