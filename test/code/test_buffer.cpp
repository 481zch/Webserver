#include <gtest/gtest.h>
#include <fcntl.h>
#include <unistd.h>
#include "buffer/buffer.h"

TEST(CircleBufferTest, AppendAndReadBytes)
{
    CircleBuffer buffer;
    
    // 测试往缓冲区中写入数据
    std::string writeIn = "Hello, World!";
    buffer.Append(writeIn);

    // 检查缓冲区中的可读数据
    std::string res = buffer.getReadableBytes();
    ASSERT_EQ(writeIn, res);
}

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
    ssize_t bytesRead = buffer.writeIn(fd);
    ASSERT_GT(bytesRead, 0);

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

TEST(CircleBufferTest, AppendOverflow)
{
    // 测试缓冲区大小限制（1024字节）
    CircleBuffer buffer(1024);
    
    std::string writeIn = std::string(1000, 'a');
    buffer.Append(writeIn);

    // 写入一个较大的字符串，超出缓冲区大小
    std::string overflow = std::string(50, 'b');
    buffer.Append(overflow);

    // 读取可读的字节，应该包含之前写入的部分数据（不会丢失）
    std::string result = buffer.getReadableBytes();
    
    // 确认结果
    ASSERT_EQ(result.substr(0, 1000), writeIn);
    ASSERT_EQ(result.substr(1000, 50), overflow);
}

TEST(CircleBufferTest, EmptyBufferRead)
{
    CircleBuffer buffer;
    
    // 尝试读取空缓冲区数据，应该返回空字符串
    std::string result = buffer.getReadableBytes();
    ASSERT_TRUE(result.empty());
}

TEST(CircleBufferTest, AppendMultipleTimes)
{
    CircleBuffer buffer;
    
    // 多次写入数据
    std::string writeIn1 = "First write.";
    std::string writeIn2 = "Second write.";
    buffer.Append(writeIn1);
    buffer.Append(writeIn2);
    
    // 读取结果应为连续的两次写入数据
    std::string result = buffer.getReadableBytes();
    ASSERT_EQ(result, writeIn1 + writeIn2);
}
