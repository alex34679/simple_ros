#ifndef MASTER_TCP_SERVER_H
#define MASTER_TCP_SERVER_H

#include <memory>
#include <string>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/TcpClient.h>
#include <grpcpp/grpcpp.h>
#include "ros_rpc.pb.h"
#include "message_graph.h"

namespace simple_ros {

// 添加结构体来保存待发送的更新数据
struct PendingUpdate {
    NodeInfo node_info;
    TopicTargetsUpdate update;
};

class MasterTcpServer {
public:
    MasterTcpServer(muduo::net::EventLoop* loop, std::shared_ptr<MessageGraph> graph);
    ~MasterTcpServer();

    void Start();
    void Stop();

    // 发送更新给指定节点
    bool SendUpdate(const std::string& node_name, const TopicTargetsUpdate& update);

private:
    // 创建临时客户端连接并发送更新
    void SendUpdateToNode(const NodeInfo& node_info, const TopicTargetsUpdate& update);
    
    // TCP客户端回调
    void OnConnection(const muduo::net::TcpConnectionPtr& conn);
    void OnWriteComplete(const muduo::net::TcpConnectionPtr& conn);

    muduo::net::EventLoop* loop_;
    muduo::net::TcpServer server_;
    std::shared_ptr<MessageGraph> graph_;  // 使用shared_ptr
    
    // 记录活跃的客户端连接和待处理的更新
    std::unordered_map<std::string, std::shared_ptr<muduo::net::TcpClient>> active_clients_;
    std::unordered_map<std::string, PendingUpdate> pending_updates_;
    std::mutex clients_mutex_;
};

} // namespace simple_ros

#endif // MASTER_TCP_SERVER_H
