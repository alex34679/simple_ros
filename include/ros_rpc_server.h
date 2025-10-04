#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <grpcpp/grpcpp.h>
#include "ros_rpc.grpc.pb.h"
#include "ros_rpc.pb.h"
#include "message_graph.h"
#include "master_tcp_server.h"

namespace simple_ros {

// ========== gRPC 服务 ==========
class RosRpcServiceImpl final : public RosRpcService::Service {
public:
    RosRpcServiceImpl(std::shared_ptr<MasterTcpServer> tcp_server, std::shared_ptr<MessageGraph> graph)
        : tcp_server_(tcp_server), graph_(graph) {}
    ~RosRpcServiceImpl() override = default;

    grpc::Status Subscribe(grpc::ServerContext* context,
                           const SubscribeRequest* request,
                           SubscribeResponse* response) override;

    grpc::Status RegisterPublisher(grpc::ServerContext* context,
                                   const RegisterPublisherRequest* request,
                                   RegisterPublisherResponse* response) override;

    grpc::Status Unsubscribe(grpc::ServerContext* context,
                             const UnsubscribeRequest* request,
                             UnsubscribeResponse* response) override;    

    grpc::Status UnregisterPublisher(grpc::ServerContext* context,
                                     const UnregisterPublisherRequest* request,
                                     UnregisterPublisherResponse* response) override;    

    // 获取节点列表
    grpc::Status GetNodes(grpc::ServerContext* context,
                          const GetNodesRequest* request,
                          GetNodesResponse* response) override;

    // 获取节点详细信息
    grpc::Status GetNodeInfo(grpc::ServerContext* context,
                             const GetNodeInfoRequest* request,
                             GetNodeInfoResponse* response) override;

    // 在RosRpcServiceImpl类中添加以下方法声明
    // 获取话题列表
    grpc::Status GetTopics(grpc::ServerContext* context,
                           const GetTopicsRequest* request,
                           GetTopicsResponse* response) override;

    // 获取话题详细信息
    grpc::Status GetTopicInfo(grpc::ServerContext* context,
                             const GetTopicInfoRequest* request,
                             GetTopicInfoResponse* response) override;

private:
    // 使用外部传入的图结构智能指针
    std::shared_ptr<MessageGraph> graph_;
    mutable std::mutex mtx_;
    std::shared_ptr<MasterTcpServer> tcp_server_;
};

class RosRpcServer {
public:
    // 修改构造函数，添加 MessageGraph 参数
    RosRpcServer(const std::string& server_address, std::shared_ptr<MasterTcpServer> tcp_server, std::shared_ptr<MessageGraph> graph);
    ~RosRpcServer();

    void Run();
    void Shutdown();

private:
    std::string server_address_;
    std::unique_ptr<grpc::Server> server_;
    RosRpcServiceImpl service_;
};

} // namespace simple_ros

