#ifndef ROS_RPC_CLIENT_H
#define ROS_RPC_CLIENT_H

#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "ros_rpc.grpc.pb.h"

namespace simple_ros {

class RosRpcClient {
public:
    explicit RosRpcClient(const std::string& server_address);
    ~RosRpcClient() = default;

    // 调用Subscribe RPC
    bool Subscribe(const std::string& topic_name,
                   const std::string& msg_type,
                   const NodeInfo& node_info,
                   SubscribeResponse* response);
                    
    // 调用RegisterPublisher RPC
    bool RegisterPublisher(const std::string& topic_name,
                          const std::string& msg_type,
                          const NodeInfo& node_info,
                          RegisterPublisherResponse* response);

    bool Unsubscribe(const std::string& topic_name,
                     const std::string& msg_type,
                     const NodeInfo& node_info,
                     UnsubscribeResponse* response);

    bool UnregisterPublisher(const std::string& topic_name,
                             const std::string& msg_type,
                             const NodeInfo& node_info,
                             UnregisterPublisherResponse* response);

    // 获取节点列表
    bool GetNodes(const std::string& filter, GetNodesResponse* response);
    
    // 获取节点详细信息
    bool GetNodeInfo(const std::string& node_name, GetNodeInfoResponse* response);                       

    // 在RosRpcClient类中添加以下方法声明
    // 获取话题列表
    bool GetTopics(const std::string& filter, GetTopicsResponse* response);
    
    // 获取话题详细信息
    bool GetTopicInfo(const std::string& topic_name, GetTopicInfoResponse* response);
private:
    std::unique_ptr<RosRpcService::Stub> stub_;
};

} // namespace simple_ros

#endif // ROS_RPC_CLIENT_H