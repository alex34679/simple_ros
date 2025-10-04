#pragma once
#include <memory>
#include <string>
#include <muduo/net/TcpClient.h>
#include <muduo/base/Logging.h>
#include "global_init.h"
#include <google/protobuf/message.h>
#include <arpa/inet.h> // htons, htonl
#include <unordered_map>
#include <vector>
#include "ros_rpc.pb.h"

using namespace simple_ros;

// 模板 Publisher，T 必须继承 google::protobuf::Message
template <typename T>
class Publisher {
public:
    Publisher(const std::string& topic);

    // 发布 protobuf 消息
    void publish(const T& msg);
    void unregister();
    ~Publisher();

private:
    void updateTargets();
    void createClient(const NodeInfo& nodeInfo);
    std::string getConnectionId(const NodeInfo& nodeInfo);

    std::string topic_;
    std::string msgType_;
    NodeInfo nodeInfo_;  // 节点信息
    std::unordered_map<std::string, std::unique_ptr<muduo::net::TcpClient>> clients_;
    std::unordered_map<std::string, muduo::net::TcpConnectionPtr> connections_;
};

// 引入模板实现
#include "publisher.inl"
