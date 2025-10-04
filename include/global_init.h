#pragma once

#include <memory>
#include <thread>
#include <muduo/net/EventLoop.h>
#include "message_queue.h"
#include "poll_manager.h"
#include "ros_rpc_client.h"  // 添加ROS RPC客户端头文件
#include <string>
#include "ros_rpc.pb.h"

using namespace simple_ros;
class SystemManager {
public:
    // 单例入口
    static SystemManager& instance();

    // 初始化系统
    void init();
    void init(int port);
    void init(int port, std::string node_name);
    void init(const std::string& node_name);
    // spin 主线程处理消息队列
    void spin();

    void spinOnce();

    // 关闭系统
    void shutdown();

    // 获取消息队列指针
    std::shared_ptr<MessageQueue> getMessageQueue() const { return messageQueue_; }

    // 获取 PollManager 指针
    std::shared_ptr<PollManager> getPollManager() const { return pollManager_; }

    // 获取 EventLoop 指针
    std::shared_ptr<muduo::net::EventLoop> getEventLoop() const { return eventLoop_; }

    // 获取全局RPC客户端
    std::shared_ptr<RosRpcClient> getRpcClient() const { return rpcClient_; }

    NodeInfo getNodeInfo() const { return nodeInfo_; }

    // 禁止拷贝
    SystemManager(const SystemManager&) = delete;
    SystemManager& operator=(const SystemManager&) = delete;


private:
    SystemManager();
    ~SystemManager();

    int findAvailablePort(int start_port = 60000, int end_port = 61000);

    std::shared_ptr<MessageQueue> messageQueue_;
    std::shared_ptr<PollManager> pollManager_;
    std::shared_ptr<muduo::net::EventLoop> eventLoop_;
    std::shared_ptr<RosRpcClient> rpcClient_;  // 全局RPC客户端
    std::thread eventThread_;
    NodeInfo nodeInfo_;  // 节点信息
    bool running_ = true;
};
