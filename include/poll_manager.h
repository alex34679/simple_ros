#pragma once
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/Buffer.h>
#include <muduo/base/Timestamp.h>
#include <string>
#include <functional>
#include "ros_rpc.pb.h"

using namespace simple_ros;

struct NodeInfoHash {
    size_t operator()(const NodeInfo& n) const {
        return std::hash<std::string>()(n.ip()) ^ (std::hash<int>()(n.port()) << 1);
    }
};

struct NodeInfoEqual {
    bool operator()(const NodeInfo& a, const NodeInfo& b) const {
        return a.ip() == b.ip() && a.port() == b.port();
    }
};

class PollManager {
public:
    PollManager(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listenAddr);
    void start();

    // 设置接收消息的回调
    void setMessageCallback(std::function<void(const std::string&, const std::string&)> cb) {
        messageCallback_ = std::move(cb);
    }


    std::unordered_set<NodeInfo, NodeInfoHash, NodeInfoEqual> getTargets(const std::string& topic) const;

private:
    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    void onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp time);
    void handleMessage(const std::string& topic, const std::string& msg_name, const std::string& data);
    muduo::net::TcpServer server_;
    std::function<void(const std::string&, const std::string&)> messageCallback_;
    std::unordered_map<std::string, std::unordered_set<NodeInfo, NodeInfoHash, NodeInfoEqual>> topic_targets_;
};
