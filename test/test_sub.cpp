#include "global_init.h"
#include "example.pb.h"
#include "node_handle.h"
#include <thread>
#include <chrono>
#include <memory>
#include <muduo/base/Logging.h>

int main() {
    // 初始化系统
    auto& sys = SystemManager::instance();
    sys.init(12345, "subcriber_node");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    NodeHandle nh;

    // 创建 Publisher 用于回发
    auto pub = nh.advertise<example::SensorData>("echo_topic");

    // 创建 Subscriber
    auto sub = nh.subscribe<example::SensorData>("test_topic", 10,
        [&](const std::shared_ptr<example::SensorData>& msg){
            LOG_INFO << "Echo received message: sensor_id=" << msg->sensor_id()
                     << ", value=" << msg->value();

            // 回发消息
            example::SensorData reply;
            reply.set_sensor_id(msg->sensor_id());
            reply.set_value(msg->value());
            pub->publish(reply);
            LOG_INFO << "Echo published message back: sensor_id=" << reply.sensor_id()
                     << ", value=" << reply.value();
        });

    // 主循环处理消息
    while (true) {
        sys.spinOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    sys.shutdown();
    return 0;
}
