#include "msg_factory.h"
#include "poll_manager.h"  // 确保包含 PollManager
#include "example.pb.h"
#include <gtest/gtest.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/base/Logging.h>
#include <thread>
#include <chrono>
#include <memory>

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

// ---------------- 消息注册宏 ----------------
#define REGISTER_MSG(TYPE) \
    MsgFactory::instance().registerMessage<example::TYPE>()
// ---------------- 测试 ----------------
TEST(PollManagerTest, ClientSendAndServerParse) {
    REGISTER_MSG(SensorData);

    // ---------------- 创建服务器 EventLoop ----------------
    muduo::net::EventLoop serverLoop;
    muduo::net::InetAddress listenAddr("127.0.0.1", 12345); // 明确使用本地回环
    PollManager server(&serverLoop, listenAddr);
    server.start();
    LOG_INFO << "Server started on " << listenAddr.toIpPort();

    // ---------------- 构造消息 ----------------
    example::SensorData sensor;
    sensor.set_sensor_id(42);
    sensor.set_value(3.14f);

    std::string data;
    ASSERT_TRUE(sensor.SerializeToString(&data));

    // 新协议增加 topic
    std::string topic_name = "sensor_topic";
    std::string msg_name = "SensorData";

    uint16_t topic_len = htons(static_cast<uint16_t>(topic_name.size()));
    uint16_t name_len  = htons(static_cast<uint16_t>(msg_name.size()));
    uint32_t msg_len   = htonl(static_cast<uint32_t>(data.size()));

    std::string buffer;
    buffer.append(reinterpret_cast<const char*>(&topic_len), sizeof(topic_len));
    buffer.append(topic_name);
    buffer.append(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
    buffer.append(msg_name);
    buffer.append(reinterpret_cast<const char*>(&msg_len), sizeof(msg_len));
    buffer.append(data);

    // ---------------- 客户端线程 ----------------
    std::thread clientThread([listenAddr, buffer]() {
        muduo::net::EventLoop clientLoop;
        muduo::net::TcpClient client(&clientLoop, listenAddr, "TestClient");

        client.setConnectionCallback([&buffer](const muduo::net::TcpConnectionPtr& conn) {
            if (conn->connected()) {
                LOG_INFO << "Client connected to server: " << conn->peerAddress().toIpPort();
                LOG_INFO << "Sending message of size: " << buffer.size();
                conn->send(buffer);
            } else {
                LOG_WARN << "Client disconnected: " << conn->name();
            }
        });

        client.setWriteCompleteCallback([](const muduo::net::TcpConnectionPtr& conn){
            LOG_INFO << "Message sent successfully: " << conn->name();
            conn->getLoop()->quit(); // 发送完成后退出客户端 EventLoop
        });

        client.connect();
        clientLoop.loop();  // 阻塞，直到 quit() 被调用
    });

    // ---------------- 服务器 EventLoop 运行一段时间 ----------------
    serverLoop.runAfter(1.0, [&serverLoop](){ 
        LOG_INFO << "Server quitting loop"; 
        serverLoop.quit(); 
    });
    serverLoop.loop();

    if (clientThread.joinable()) clientThread.join();

    // ---------------- 验证消息 ----------------
    auto msg = MsgFactory::instance().createMessage("SensorData");
    ASSERT_TRUE(msg);
    ASSERT_TRUE(msg->ParseFromString(data));
    EXPECT_EQ(getField<int32_t>(*msg, "sensor_id"), 42);
    EXPECT_FLOAT_EQ(getField<float>(*msg, "value"), 3.14f);
}
