#include "global_init.h"
#include "msg_factory.h"
#include "example.pb.h"
#include <gtest/gtest.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Logging.h>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>
#include "node_handle.h"

// ---------------- 测试 ----------------
TEST(SystemManagerTest, InitAndSendMessage) {
    // 初始化系统
    auto& sys = SystemManager::instance();
    sys.init();
    LOG_INFO << "System initialized successfully";

    // 等待系统完全启动
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 注册消息类型
    MsgFactory::instance().registerMessage<example::SensorData>();
    LOG_INFO << "Message type 'SensorData' registered";

    // 注册topic
    const std::string topic_name = "test_topic";

    // 设置消息回调
    std::atomic<bool> message_received(false);
    std::atomic<int32_t> received_sensor_id(-1);
    std::atomic<float> received_value(-1.0f);

    NodeHandle nh;
    auto test_sub = nh.subscribe<example::SensorData>(topic_name, 10,  // 队列大小设置为100
        [&](const std::shared_ptr<example::SensorData>& sensor_data) {
            LOG_INFO << "Received message on topic '" << topic_name << "'";
            message_received = true;
            received_sensor_id = sensor_data->sensor_id();
            received_value = sensor_data->value();
            LOG_INFO << "Sensor ID: " << received_sensor_id 
                     << ", Value: " << received_value;
        });

    // 构造消息
    example::SensorData sensor;
    sensor.set_sensor_id(100);
    sensor.set_value(2.718f);

    std::string data;
    ASSERT_TRUE(sensor.SerializeToString(&data));

    std::string msg_name = "example.SensorData";
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

    // 创建客户端并发送消息
    muduo::net::InetAddress serverAddr("127.0.0.1", 12345);
    muduo::net::EventLoop clientLoop;
    muduo::net::TcpClient client(&clientLoop, serverAddr, "InitTestClient");

    bool client_connected = false;
    client.setConnectionCallback([&](const muduo::net::TcpConnectionPtr& conn) {
        if (conn->connected()) {
            LOG_INFO << "Client connected to server: " << conn->peerAddress().toIpPort();
            client_connected = true;
            conn->send(buffer);
        } else {
            LOG_WARN << "Client disconnected: " << conn->name();
        }
    });

    client.setWriteCompleteCallback([&clientLoop](const muduo::net::TcpConnectionPtr& conn){
        LOG_INFO << "Message sent successfully: " << conn->name();
        clientLoop.quit(); // 发送完成后退出客户端 EventLoop
    });

    client.connect();
    clientLoop.loop();

    // 等待消息处理
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    LOG_INFO << "Message begin: ";
    sys.spinOnce();
    LOG_INFO << "Message end: " ;
    // 验证消息是否被正确接收和处理
    EXPECT_TRUE(message_received);
    EXPECT_EQ(received_sensor_id, 100);
    EXPECT_FLOAT_EQ(received_value, 2.718f);
    // 关闭系统
    // sys.shutdown();

    LOG_INFO << "Test completed";
}
