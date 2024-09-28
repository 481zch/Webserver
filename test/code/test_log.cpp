#include <gtest/gtest.h>
#include <fcntl.h>
#include <unistd.h>
#include "log/log.h"

TEST(LogTest, TestLogDocument)
{
    Log* log = Log::getInstance();
    log->LOG_RECORD(0, "你好");
}