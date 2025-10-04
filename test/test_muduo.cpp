#include <gtest/gtest.h>
#include <muduo/base/Logging.h>

TEST(MuduoTest, LoggingWorks) {
    testing::internal::CaptureStdout();
    LOG_INFO << "Hello from muduo";
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("Hello from muduo"), std::string::npos);
}
