#include <gtest/gtest.h>
#include <fcntl.h>
#include <random>
#include <unistd.h>
#include "buffer/buffer.h"

// 测试写入和读取字符串数据
TEST(CircleBufferTest, AppendAndReadBytes)
{
    CircleBuffer buffer;

    // 测试往缓冲区中写入数据
    std::string writeIn = "Hello, World!";
    int err = 0;
    buffer.Append(writeIn, &err);

    // 检查缓冲区中的可读数据
    std::string res = buffer.getReadableBytes();
    ASSERT_EQ(writeIn, res);
    ASSERT_EQ(err, 0);
}

// 测试从文件中读取数据并写回文件
TEST(CircleBufferTest, writeAndReadFile)
{
    CircleBuffer buffer;

    // 创建并打开测试文件
    int fd = open("./tmp/tmp_file.txt", O_RDWR | O_CREAT | O_TRUNC, 0666);
    ASSERT_NE(fd, -1);

    // 模拟文件中写入的数据
    std::string writeIn = "Test data for CircleBuffer.";
    ASSERT_EQ(write(fd, writeIn.c_str(), writeIn.size()), writeIn.size());

    // 将文件偏移指针重置到文件开头
    lseek(fd, 0, SEEK_SET);

    // 从文件中读取数据到缓冲区
    int err = 0;
    ssize_t bytesRead = buffer.writeIn(fd, &err);
    ASSERT_GT(bytesRead, 0);
    ASSERT_EQ(err, 0);

    // 从缓冲区中读取数据并写入文件
    lseek(fd, 0, SEEK_SET);
    ssize_t bytesWritten = buffer.readOut(fd);
    ASSERT_EQ(bytesWritten, bytesRead);

    // 确认文件中的数据是否与写入的一致
    char fileContent[100] = {0};
    lseek(fd, 0, SEEK_SET);
    ASSERT_EQ(read(fd, fileContent, bytesWritten), bytesWritten);
    ASSERT_STREQ(fileContent, writeIn.c_str());

    close(fd);
}

// 测试缓冲区写入超出限制的情况
TEST(CircleBufferTest, AppendOverflow)
{
    // 测试缓冲区大小限制（1024字节）
    CircleBuffer buffer(1024);

    std::string writeIn = std::string(1000, 'a');
    int err = 0;
    buffer.Append(writeIn, &err);

    // 写入一个较大的字符串，超出缓冲区大小
    std::string overflow = std::string(50, 'b');
    buffer.Append(overflow, &err);

    // 读取可读的字节，应该包含之前写入的部分数据（不会丢失）
    std::string result = buffer.getReadableBytes();

    // 确认结果
    ASSERT_EQ(result.substr(0, 1000), writeIn);
}

// 测试从空缓冲区读取数据
TEST(CircleBufferTest, EmptyBufferRead)
{
    CircleBuffer buffer;

    // 尝试读取空缓冲区数据，应该返回空字符串
    std::string result = buffer.getReadableBytes();
    ASSERT_TRUE(result.empty());
}

// 测试多次写入数据
TEST(CircleBufferTest, AppendMultipleTimes)
{
    CircleBuffer buffer;

    // 多次写入数据
    std::string writeIn1 = "First write.";
    std::string writeIn2 = "Second write.";
    int err = 0;
    buffer.Append(writeIn1, &err);
    buffer.Append(writeIn2, &err);

    // 读取结果应为连续的两次写入数据
    std::string result = buffer.getReadableBytes();
    ASSERT_EQ(result, writeIn1 + writeIn2);
}

// 测试通过结束符读取数据
TEST(CircleBufferTest, ReadByEndBytes)
{
    CircleBuffer buffer(1024, "\r\n");

    // 模拟往缓冲区中写入数据
    std::string writeIn1 = "Line 1\r\n";
    std::string writeIn2 = "Line 2\r\n";
    int err = 0;
    buffer.Append(writeIn1, &err);
    buffer.Append(writeIn2, &err);

    // 按照结束符读取第一行
    std::string result1 = buffer.getByEndBytes(&err);
    ASSERT_EQ(result1, writeIn1);
    ASSERT_EQ(err, 0);

    // 读取第二行
    std::string result2 = buffer.getByEndBytes(&err);
    ASSERT_EQ(result2, writeIn2);
    ASSERT_EQ(err, 0);
}


std::string generateRandomString(size_t length) {
    const char charset[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::default_random_engine rng(std::random_device{}());
    std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += charset[dist(rng)];
    }
    return result;
}

TEST(CircleBufferTest, RandomAppendAndRead) {
    CircleBuffer buffer(1024, "\r\n"); // 初始化一个固定大小的缓冲区
    const size_t numIterations = 100; // 测试循环次数
    const size_t maxStringLength = 256; // 生成字符串的最大长度

    std::string totalData; // 用来保存所有写入的字符串
    int err = 0; // 错误标志位

    for (size_t i = 0; i < numIterations; ++i) {
        // 生成随机长度的字符串
        size_t randLength = rand() % maxStringLength + 1;
        std::string randomStr = generateRandomString(randLength);
        
        // Append 随机生成的字符串到环形缓冲区
        buffer.Append(randomStr, &err);
        ASSERT_EQ(err, 0); // 确保Append成功
        
        // 将写入的字符串加入到 totalData，用于验证
        totalData += randomStr;

        // 从缓冲区读取可读的数据并验证
        std::string readData = buffer.getReadableBytes();
        ASSERT_EQ(readData, randomStr);
        printf("第%d次测试结果：\n", i);
        std::cout << "生成的字符串为：" << randomStr << std::endl;
        std::cout << "读取的字符串为：" << randomStr << std::endl;

    }
}