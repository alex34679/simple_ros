#include "global_init.h"
#include "example.pb.h"
#include "node_handle.h"
#include <thread>
#include <chrono>
#include <memory>
#include <muduo/base/Logging.h>

// 发布者节点类
class PublisherNode {
public:
    PublisherNode(int port, const std::string& nodeName) 
        : port_(port), nodeName_(nodeName), counter_(0) {}
    
    // 初始化节点
    void initialize() {
        // 初始化系统
        auto& sys = SystemManager::instance();
        sys.init(port_, nodeName_);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // 创建NodeHandle
        nh_ = std::make_shared<NodeHandle>();
        
        // 创建发布者
        pub_ = nh_->advertise<example::SensorData>("test_topic");
        
        // 创建订阅者
        sub_ = nh_->subscribe<example::SensorData>("echo_topic", 10,
            std::bind(&PublisherNode::echoCallback, this, std::placeholders::_1));
        
        // 创建定时器，使用类成员函数作为回调
        timer_ = nh_->createTimer(1.0, 
            std::bind(&PublisherNode::timerCallback, this, std::placeholders::_1), 
            false);  // false表示周期性定时器
    }
    
    // 启动节点
    void run() {
        // 运行事件循环
        auto& sys = SystemManager::instance();
        sys.spin();  // 阻塞主线程，处理所有事件
    }
    
    // 关闭节点
    void shutdown() {
        auto& sys = SystemManager::instance();
        sys.shutdown();
    }
    
private:
    int port_;                          // 端口号
    std::string nodeName_;              // 节点名称
    int counter_;                       // 计数器
    std::shared_ptr<NodeHandle> nh_;    // NodeHandle指针
    std::shared_ptr<Publisher<example::SensorData>> pub_;  // 发布者指针
    std::shared_ptr<Subscriber> sub_;   // 订阅者指针
    std::shared_ptr<Timer> timer_;      // 定时器指针
    
    // 定时器回调函数
    void timerCallback(const TimerEvent& event) {
        // 创建并发布消息
        example::SensorData sensor;
        sensor.set_sensor_id(100 + counter_);
        sensor.set_value(3.14f + counter_);
        
        pub_->publish(sensor);
        LOG_INFO << "Published message: sensor_id=" << sensor.sensor_id()
                 << ", value=" << sensor.value()
                 << ", counter=" << counter_;
        
        // 递增计数器（可以根据需要重置或限制范围）
        counter_++;
        // 如果需要循环计数，可以使用模运算
        // counter_ = (counter_ + 1) % 100;  // 例如，限制在0-99范围内
    }
    
    // 订阅回调函数
    void echoCallback(const std::shared_ptr<example::SensorData>& msg) {
        LOG_INFO << "Echo received message: sensor_id=" << msg->sensor_id()
                 << ", value=" << msg->value();
    }
};

int main() {
    // 创建并初始化发布者节点
    PublisherNode node(12346, "publisher_node");
    node.initialize();
    
    // 运行节点
    node.run();
    
    // 注意：由于run()会调用spin()阻塞，以下代码通常不会执行到
    node.shutdown();
    return 0;
}
