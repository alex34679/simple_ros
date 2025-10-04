#include "msg_factory.h"
#include "example.pb.h"
#include <iostream>
#include <gtest/gtest.h>
#include <stdexcept>
#include <type_traits>

// ---------------- 工具函数 ----------------
template<typename T>
T getField(const google::protobuf::Message& msg, const std::string& field_name) {
    const auto* desc = msg.GetDescriptor();
    const auto* refl = msg.GetReflection();
    const auto* field = desc->FindFieldByName(field_name);
    if (!field) throw std::runtime_error("Field not found: " + field_name);

    if constexpr (std::is_same_v<T, int32_t>) {
        return refl->GetInt32(msg, field);
    } else if constexpr (std::is_same_v<T, float>) {
        return refl->GetFloat(msg, field);
    } else if constexpr (std::is_same_v<T, std::string>) {
        return refl->GetString(msg, field);
    } else {
        static_assert(sizeof(T) == 0, "Unsupported field type");
    }
}

void processSensorData(const example::SensorData& data) {
    std::cout << "处理传感器数据: ID=" << data.sensor_id() 
              << ", 值=" << data.value() << std::endl; 
}

// ---------------- 消息注册宏 ----------------
#define REGISTER_MSG(TYPE) \
    MsgFactory::instance().registerMessage<example::TYPE>()
// ---------------- 测试 ----------------
TEST(MsgFactoryTest, CreateAndParse) {
    // 注册消息
    REGISTER_MSG(SensorData);
    REGISTER_MSG(ControlCommand);

    // 创建消息
    auto sensor_msg = MsgFactory::instance().createMessage("example.SensorData");
    ASSERT_NE(sensor_msg, nullptr);

    std::cout << "Created message type: " \
            << sensor_msg->GetTypeName() << std::endl;

    // 构造原始消息并序列化
    example::SensorData sensor;
    sensor.set_sensor_id(100);
    sensor.set_value(12.34f);

    std::string data;
    sensor.SerializeToString(&data);


    // 解析到动态消息
    ASSERT_TRUE(sensor_msg->ParseFromString(data));

    // 转换为具体类型并调用处理函数
    auto* concrete_sensor = dynamic_cast<example::SensorData*>(sensor_msg.get());
    ASSERT_NE(concrete_sensor, nullptr);
    processSensorData(*concrete_sensor);

    // 使用 Reflection 断言字段值
    EXPECT_EQ(getField<int32_t>(*sensor_msg, "sensor_id"), 100);
    EXPECT_FLOAT_EQ(getField<float>(*sensor_msg, "value"), 12.34f);
}
