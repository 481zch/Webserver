#include "buffer/linearBuffer.h"
#include <gtest/gtest.h>

TEST(BufferTest, TestAppend)
{
    LinearBuffer buffer;
    std::string testText = "GET /css/bootstrap.min.css HTTP/1.1\r\nHost: 192.168.19.133:1317\r\nConnection: keep-alive\r\nsec-ch-ua-platform: \"Windows\"\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36\r\nsec-ch-ua: \"Chromium\";v=\"130\", \"Google Chrome\";v=\"130\", \"Not?A_Brand\";v=\"99\"\r\nsec-ch-ua-mobile: ?0\r\nAccept: text/css,*/*;q=0.1\r\nSec-Fetch-Site: same-origin\r\nSec-Fetch-Mode: no-cors\r\nSec-Fetch-Dest: style\r\nReferer: https://192.168.19.133:1317/\r\nAccept-Encoding: gzip, deflate, br, zstd\r\nAccept-Language: zh-CN,zh;q=0.9,en;q=0.8\r\n\r\n";
    buffer.append(testText);
    std::string takeOut = buffer.getReadAbleBytes();
    ASSERT_EQ(testText, takeOut);

    // 测试尾结束符是否正确
    buffer.append(testText);
    std::string temp;
    std::string end = "\r\n";
    while (!(temp = buffer.getByEndFlag("\r\n")).empty()) {
        std::cout << temp << std::endl;
    }

    size_t resetBytes = buffer.readAbleBytes();
    ASSERT_EQ(resetBytes, 0);
}