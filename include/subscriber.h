#pragma once
// 确保包含必要的头文件
#include <string>
#include <memory>
#include <functional>
#include <google/protobuf/message.h>
#include "global_init.h"  // 添加这个以获取SystemManager
#include "message_queue.h"
#include "ros_rpc.pb.h"  // 添加protobuf头文件

// 前向声明
class SystemManager;
class MessageQueue;

/**
 * @brief Subscriber类，用于管理订阅的生命周期
 * 当实例销毁时，会自动从消息队列中移除对应的订阅
 */
class Subscriber {
public:
    /**
     * @brief 构造函数
     * @param topic 主题名称
     * @param queue_size 队列大小
     * @param callback 回调函数
     */
    Subscriber(const std::string& topic, 
               uint32_t queue_size, 
               MessageQueue::Callback callback);

    /**
     * @brief 类型安全的模板构造函数
     * @tparam MsgType 消息类型
     * @param topic 主题名称
     * @param queue_size 队列大小
     * @param typed_callback 类型化的回调函数
     */
    template<typename MsgType>
    Subscriber(const std::string& topic, 
                uint32_t queue_size, 
                std::function<void(const std::shared_ptr<MsgType>&)> typed_callback);

    /**
     * @brief 析构函数，自动取消订阅
     */
    ~Subscriber();

    // 禁止拷贝
    Subscriber(const Subscriber&) = delete;
    Subscriber& operator=(const Subscriber&) = delete;

    // 允许移动
    Subscriber(Subscriber&&) noexcept = delete;
    Subscriber& operator=(Subscriber&&) noexcept = delete;

private:
    std::string topic_;                 // 主题名称
    uint32_t queue_size_;               // 队列大小
    MessageQueue::Callback callback_;   // 回调函数
    std::weak_ptr<MessageQueue> msg_queue_; // 弱引用，避免循环引用
    std::string msg_type_;              // 消息类型
    simple_ros::NodeInfo node_info_;      // 节点信息
};


// 模板构造函数实现
#include "subscriber.inl"