#include <gtest/gtest.h>
#include "message.pb.h"

TEST(ProtobufTest, SerializeDeserialize) {
    ustc::Pose pose;
    pose.set_x(1.23);
    pose.set_y(4.56);
    pose.set_z(7.89);

    std::string data;
    ASSERT_TRUE(pose.SerializeToString(&data));

    ustc::Pose pose2;
    ASSERT_TRUE(pose2.ParseFromString(data));

    EXPECT_DOUBLE_EQ(pose2.x(), 1.23);
    EXPECT_DOUBLE_EQ(pose2.y(), 4.56);
    EXPECT_DOUBLE_EQ(pose2.z(), 7.89);
}
