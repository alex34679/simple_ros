#pragma once
#include "publisher.h"
#include "poll_manager.h"
#include <muduo/net/InetAddress.h>
#include "ros_rpc.pb.h"
#include <muduo/base/Logging.h>
#include <muduo/net/TcpClient.h>
#include "global_init.h"
#include "msg_factory.h"

using namespace simple_ros;
// 构造函数

template <typename T>
Publisher<T>::Publisher(const std::string& topic) : topic_(topic) {
    // 获取消息类型
    msgType_ = T::descriptor()->full_name();
    LOG_INFO << "Creating publisher for topic: " << topic_ << ", type: " << msgType_;

    // 从系统管理器获取节点信息
    nodeInfo_ = SystemManager::instance().getNodeInfo();

    // 初始化时更新目标节点
    updateTargets();
}

// 析构函数

template <typename T>
Publisher<T>::~Publisher() {
    unregister();
}

// 取消注册发布者

template <typename T>
void Publisher<T>::unregister() {
    LOG_INFO << "Unregistering publisher for topic: " << topic_;

    // 调用RPC取消注册
    auto rpc_client = SystemManager::instance().getRpcClient();
    if (rpc_client) {
        simple_ros::UnregisterPublisherResponse response;
        bool success = rpc_client->UnregisterPublisher(topic_, msgType_, nodeInfo_, &response);
        if (success) {
            LOG_INFO << "UnregisterPublisher RPC successful for topic: " << topic_;
        } else {
            LOG_ERROR << "UnregisterPublisher RPC failed for topic: " << topic_;
        }
    } else {
        LOG_ERROR << "Global RPC client not initialized";
    }

    // 清理客户端连接
    clients_.clear();
    connections_.clear();
}

// 发布消息

template <typename T>
void Publisher<T>::publish(const T& msg) {
    // 确保目标节点是最新的
    updateTargets();

    // 序列化消息
    std::string msg_data;
    if (!msg.SerializeToString(&msg_data)) {
        LOG_ERROR << "Failed to serialize message of type: " << msgType_;
        return;
    }

    // 按照协议格式构建完整消息：topic_name_len(2B) + topic_name + msg_name_len(2B) + msg_name + msg_data_len(4B) + msg_data
    std::string buffer;
    
    // 1. 写入 topic 名称长度和 topic 名称
    uint16_t topic_len = htons(static_cast<uint16_t>(topic_.size()));
    buffer.append(reinterpret_cast<const char*>(&topic_len), sizeof(topic_len));
    buffer.append(topic_);
    
    // 2. 写入消息名称长度和消息名称
    uint16_t msg_name_len = htons(static_cast<uint16_t>(msgType_.size()));
    buffer.append(reinterpret_cast<const char*>(&msg_name_len), sizeof(msg_name_len));
    buffer.append(msgType_);
    
    // 3. 写入消息数据长度和消息数据
    uint32_t msg_data_len = htonl(static_cast<uint32_t>(msg_data.size()));
    buffer.append(reinterpret_cast<const char*>(&msg_data_len), sizeof(msg_data_len));
    buffer.append(msg_data);

    // 为每个连接发送消息
    for (const auto& conn_pair : connections_) {
        if (conn_pair.second && conn_pair.second->connected()) {
            // 增加调试信息，打印目标端口
            // LOG_INFO << "Publishing message to topic: " << topic_ 
            //          << ", target port: " << conn_pair.second->peerAddress().port()
            //          << ", message type: " << msgType_;
            conn_pair.second->send(buffer);
        }
    }
}

// 更新目标节点

template <typename T>
void Publisher<T>::updateTargets() {
    auto poll_manager = SystemManager::instance().getPollManager();
    if (!poll_manager) {
        LOG_ERROR << "PollManager not initialized";
        return;
    }

    // 获取订阅该主题的所有节点

    auto targets_set = poll_manager->getTargets(topic_);
    std::vector<NodeInfo> targets(targets_set.begin(), targets_set.end());
    // 为每个新节点创建客户端
    for (const auto& node_info : targets) {
        std::string conn_id = getConnectionId(node_info);
        if (clients_.find(conn_id) == clients_.end()) {
            createClient(node_info);
        }
    }
}

// 创建TCP客户端

template <typename T>
void Publisher<T>::createClient(const NodeInfo& nodeInfo) {
    std::string conn_id = getConnectionId(nodeInfo);
    LOG_INFO << "Creating TCP client for: " << conn_id;

    muduo::net::InetAddress server_addr(nodeInfo.ip().c_str(), nodeInfo.port());
    auto client = std::make_unique<muduo::net::TcpClient>(
        SystemManager::instance().getEventLoop().get(),
        server_addr,
        "PublisherClient"
    );

    // 设置连接回调
    client->setConnectionCallback([this, conn_id](const muduo::net::TcpConnectionPtr& conn) {
        if (conn->connected()) {
            LOG_INFO << "Connected to " << conn_id;
            connections_[conn_id] = conn;
        } else {
            LOG_INFO << "Disconnected from " << conn_id;
            connections_.erase(conn_id);
        }
    });

    // 连接服务器
    client->connect();
    clients_[conn_id] = std::move(client);
}

// 获取连接ID

template <typename T>
std::string Publisher<T>::getConnectionId(const NodeInfo& nodeInfo) {
    return nodeInfo.ip() + ":" + std::to_string(nodeInfo.port());
}
