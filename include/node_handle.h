#pragma once

#include <string>
#include <memory>
#include <functional>
#include "subscriber.h"
#include "publisher.h"
#include "ros_rpc.pb.h"  // 添加protobuf头文件
#include "timer.h"       // 添加timer头文件

using namespace simple_ros;

/**
 * @brief NodeHandle类，用于管理ROS节点和订阅者
 * 提供创建订阅者的接口，避免与SystemManager的循环依赖
 */
class NodeHandle {
public:
    /**
     * @brief 构造函数
     */
    NodeHandle();

    /**
     * @brief 析构函数
     */
    ~NodeHandle();

    // 禁止拷贝
    NodeHandle(const NodeHandle&) = delete;
    NodeHandle& operator=(const NodeHandle&) = delete;

    // 允许移动
    NodeHandle(NodeHandle&&) noexcept;
    NodeHandle& operator=(NodeHandle&&) noexcept;

    /**
     * @brief 创建订阅者(函数对象版本)
     * @tparam MsgType 消息类型
     * @param topic 主题名称
     * @param queue_size 队列大小
     * @param callback 回调函数
     * @return Subscriber实例的共享指针
     */
    template<typename MsgType>
    std::shared_ptr<Subscriber> subscribe(const std::string& topic, 
                                         uint32_t queue_size, 
                                         std::function<void(const std::shared_ptr<MsgType>&)> callback);

    /**
     * @brief 创建订阅者(类成员函数版本)
     * @tparam MsgType 消息类型
     * @tparam Class 类类型
     * @param topic 主题名称
     * @param queue_size 队列大小
     * @param callback 类成员函数指针
     * @param instance 类实例指针
     * @return Subscriber实例的共享指针
     */
    template<typename MsgType, typename Class>
    std::shared_ptr<Subscriber> subscribe(const std::string& topic, 
                                         uint32_t queue_size, 
                                         void(Class::*callback)(const std::shared_ptr<MsgType>&), 
                                         Class* instance);

    /**
     * @brief 创建订阅者(非模板版本，通过字符串消息类型)
     * @param topic 主题名称
     * @param queue_size 队列大小
     * @param msg_type_name 消息类型名称
     * @param callback 回调函数
     * @return Subscriber实例的共享指针
     */
    std::shared_ptr<Subscriber> subscribe(const std::string& topic, 
                                         uint32_t queue_size, 
                                         const std::string& msg_type_name, 
                                         MessageQueue::Callback callback);

    /**
     * @brief 创建发布者
     * @tparam MsgType protobuf消息类型
     * @param topic 主题名称
     * @return Publisher<MsgType> 的共享指针
     */
    template<typename MsgType>
    std::shared_ptr<Publisher<MsgType>> advertise(const std::string& topic);

    /**
     * @brief 创建定时器
     * @param period 定时器周期（秒）
     * @param callback 回调函数
     * @param oneshot 是否为一次性定时器
     * @return Timer实例的共享指针
     */
    std::shared_ptr<Timer> createTimer(double period, const TimerCallback& callback, bool oneshot = false);

private:
    NodeInfo nodeInfo_;  // 节点信息
};

// 模板方法实现
#include "node_handle.inl"