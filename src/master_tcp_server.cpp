#include "master_tcp_server.h"
#include <iostream>
#include <arpa/inet.h> // htons htonl
#include <muduo/base/Logging.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <boost/bind/bind.hpp>
#include <sstream>
#include "master_tcp_server.h"
#include "ros_rpc.pb.h"
#include <muduo/base/Logging.h>

namespace simple_ros {

MasterTcpServer::MasterTcpServer(muduo::net::EventLoop* loop, std::shared_ptr<MessageGraph> graph)
    : loop_(loop), graph_(graph), server_(loop, muduo::net::InetAddress(50052), "MasterTcpServer") {
    LOG_INFO << "MasterTcpServer initialized";
}

MasterTcpServer::~MasterTcpServer() {
    Stop();
    LOG_INFO << "MasterTcpServer destroyed";
}

void MasterTcpServer::Start() {
    LOG_INFO << "MasterTcpServer started";
}

void MasterTcpServer::Stop() {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    active_clients_.clear();
    pending_updates_.clear();
    LOG_INFO << "MasterTcpServer stopped";
}

bool MasterTcpServer::SendUpdate(const std::string& node_name, const TopicTargetsUpdate& update) {
    NodeInfo node_info;
    if (!graph_->GetNodeByName(node_name, &node_info)) {
        LOG_WARN << "Failed to send update: node not found - " << node_name;
        return false;
    }
    
    // 在IO线程中异步执行发送操作
    loop_->runInLoop(boost::bind(&MasterTcpServer::SendUpdateToNode, this, node_info, update));
    return true;
}

void MasterTcpServer::SendUpdateToNode(const NodeInfo& node_info, const TopicTargetsUpdate& update) {
    if (node_info.ip().empty() || node_info.port() <= 0) {
        LOG_WARN << "Invalid node address - ip: " << node_info.ip() << ", port: " << node_info.port();
        return;
    }
    
    muduo::net::InetAddress peer_addr(node_info.ip(), static_cast<uint16_t>(node_info.port()));
    std::string peer_key = peer_addr.toIpPort();
    
    LOG_INFO << "Creating temporary connection to node: " << node_info.node_name() 
             << " at " << peer_key << " for topic: " << update.topic();
    
    // 存储待发送的更新数据
    {    
        std::lock_guard<std::mutex> lock(clients_mutex_);
        pending_updates_[peer_key] = {node_info, update};
    }
    
    // 创建临时TcpClient
    auto client = std::make_shared<muduo::net::TcpClient>(
        loop_, peer_addr, "MasterTcpClient-" + node_info.node_name());
    
    client->setConnectionCallback(
        boost::bind(&MasterTcpServer::OnConnection, this, boost::placeholders::_1));
    client->setWriteCompleteCallback(
        boost::bind(&MasterTcpServer::OnWriteComplete, this, boost::placeholders::_1));
    
    // 保存客户端指针，避免提前销毁
    {    
        std::lock_guard<std::mutex> lock(clients_mutex_);
        active_clients_[peer_key] = client;
    }
    
    // 连接并发送数据
    client->connect();
}

void MasterTcpServer::OnConnection(const muduo::net::TcpConnectionPtr& conn) {
    LOG_INFO << "Connection to " << conn->peerAddress().toIpPort() << " is " 
             << (conn->connected() ? "UP" : "DOWN");
    
    if (conn->connected()) {
        std::string peer_key = conn->peerAddress().toIpPort();
        PendingUpdate pending_update;
        
        // 获取待发送的更新数据
        {    
            std::lock_guard<std::mutex> lock(clients_mutex_);
            auto it = pending_updates_.find(peer_key);
            if (it != pending_updates_.end()) {
                pending_update = it->second;
                // 移除已处理的更新
                pending_updates_.erase(it);
            } else {
                LOG_WARN << "No pending update found for " << peer_key;
                return;
            }
        }
        
        // 序列化更新数据
        std::string message;
        if (!pending_update.update.SerializeToString(&message)) {
            LOG_WARN << "Failed to serialize update message";
            return;
        }
        
        // 按照协议格式构建完整消息：topic_name_len(2B) + topic_name + msg_name_len(2B) + msg_name + msg_data_len(4B) + msg_data
        std::string buffer;
        
        // 1. 写入 topic 名称长度和 topic 名称
        std::string topic = pending_update.update.topic();
        if (topic.empty()) {
            topic = "__master_topic_update";
        }
        uint16_t topic_len = htons(static_cast<uint16_t>(topic.size()));
        buffer.append(reinterpret_cast<const char*>(&topic_len), sizeof(topic_len));
        buffer.append(topic);
        
        // 2. 写入消息名称长度和消息名称
        std::string msg_name = "TopicTargetsUpdate";
        uint16_t msg_name_len = htons(static_cast<uint16_t>(msg_name.size()));
        buffer.append(reinterpret_cast<const char*>(&msg_name_len), sizeof(msg_name_len));
        buffer.append(msg_name);
        
        // 3. 写入消息数据长度和消息数据
        uint32_t msg_data_len = htonl(static_cast<uint32_t>(message.size()));
        buffer.append(reinterpret_cast<const char*>(&msg_data_len), sizeof(msg_data_len));
        buffer.append(message);
        
        // 发送完整的格式化消息
        conn->send(buffer);
    } else {
        // 连接断开，清理客户端
        std::lock_guard<std::mutex> lock(clients_mutex_);
        active_clients_.erase(conn->peerAddress().toIpPort());
    }
}

void MasterTcpServer::OnWriteComplete(const muduo::net::TcpConnectionPtr& conn) {
    LOG_INFO << "Write complete to " << conn->peerAddress().toIpPort();
    // 发送完成后主动断开连接
    conn->shutdown();
}

} // namespace simple_ros
