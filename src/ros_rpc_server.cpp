#include "ros_rpc_server.h"
#include <grpcpp/server_builder.h>
#include <iostream>
#include <muduo/base/Logging.h>

namespace simple_ros {

// ========== 服务实现 ==========

grpc::Status RosRpcServiceImpl::Subscribe(grpc::ServerContext*,
                                          const SubscribeRequest* request,
                                          SubscribeResponse* response) {
    const std::string topic_key = request->topic_name() + "/" + request->msg_type();
    const NodeInfo& node = request->node_info();

    LOG_INFO << "Received Subscribe request: topic=" << request->topic_name() 
             << ", msg_type=" << request->msg_type() 
             << ", node_name=" << node.node_name();

    {
        // 更新图
        graph_->AddSubscriber(node, {request->topic_name(), request->msg_type()});
        LOG_DEBUG << "Added subscriber " << node.node_name() << " to topic " << request->topic_name();

        // 通知该 topic 的发布者新增订阅者
        simple_ros::TopicTargetsUpdate update;
        update.set_topic(request->topic_name());
        auto* add_node = update.add_add_targets();
        *add_node = node;

        int count = 0;
        for (auto& pub : graph_->GetPublishersByTopic(request->topic_name())) {
            tcp_server_->SendUpdate(pub.node_name(), update);
            count++;
        }
        LOG_INFO << "Notified " << count << " publishers about new subscriber " << node.node_name();
    }

    response->set_success(true);
    response->set_message("Subscribe success");
    LOG_INFO << "Subscribe request processed successfully for node " << node.node_name();
    return grpc::Status::OK;
}

grpc::Status RosRpcServiceImpl::Unsubscribe(grpc::ServerContext*,
                                            const UnsubscribeRequest* request,
                                            UnsubscribeResponse* response) {
    const TopicKey k{request->topic_name(), request->msg_type()};
    const NodeInfo& node = request->node_info();

    {
        // std::lock_guard<std::mutex> lk(mtx_);
        graph_->RemoveSubscriber(node, k);

        simple_ros::TopicTargetsUpdate update;
        update.set_topic(request->topic_name());
        auto* rem_node = update.add_remove_targets();
        *rem_node = node;

        // 通知所有发布者
        for (auto& pub : graph_->GetPublishersByTopic(request->topic_name())) {
            tcp_server_->SendUpdate(pub.node_name(), update);
        }
    }

    response->set_success(true);
    response->set_message("Unsubscribe success");
    return grpc::Status::OK;
}

// RegisterPublisher / UnregisterPublisher 同理
// 当发布者增加/减少时，通知所有订阅者
grpc::Status RosRpcServiceImpl::RegisterPublisher(grpc::ServerContext*,
                                                  const RegisterPublisherRequest* request,
                                                  RegisterPublisherResponse* response) {
    const TopicKey k{request->topic_name(), request->msg_type()};
    const NodeInfo& node = request->node_info();

    {
        graph_->AddPublisher(node, k);

        LOG_INFO << "RegisterPublisher request: topic=" << request->topic_name() 
            << ", msg_type=" << request->msg_type() 
            << ", node_name=" << node.node_name();

        // 通知当前注册的发布者所有订阅该话题的节点
        simple_ros::TopicTargetsUpdate update;
        update.set_topic(request->topic_name());
        for (const auto& sub : graph_->GetSubscribersByTopic(request->topic_name())) {
            auto* add_node = update.add_add_targets();
            *add_node = sub;
        }
        
        // 只通知新注册的发布者
        tcp_server_->SendUpdate(node.node_name(), update);
    }

    response->set_success(true);
    response->set_message("Register publisher success");
    return grpc::Status::OK;
}

// 取消注册发布者时不需要通知
grpc::Status RosRpcServiceImpl::UnregisterPublisher(grpc::ServerContext*,
                                                   const UnregisterPublisherRequest* request,
                                                   UnregisterPublisherResponse* response) {
    const TopicKey k{request->topic_name(), request->msg_type()};
    const NodeInfo& node = request->node_info();

    {
        // std::lock_guard<std::mutex> lk(mtx_);
        graph_->RemovePublisher(node, k);

        LOG_INFO << "UnregisterPublisher request: topic=" << request->topic_name() 
            << ", msg_type=" << request->msg_type() 
            << ", node_name=" << node.node_name();

        // 移除所有通知逻辑
        // 不再通知订阅者
    }

    response->set_success(true);
    response->set_message("Unregister publisher success");
    return grpc::Status::OK;
}



// 获取节点列表
grpc::Status RosRpcServiceImpl::GetNodes(grpc::ServerContext*,
                                        const GetNodesRequest* request,
                                        GetNodesResponse* response) {
    std::vector<NodeInfo> nodes = graph_->GetAllNodes();
    
    for (const auto& node : nodes) {
        // 可以根据filter进行过滤，如果提供了filter参数
        if (!request->filter().empty() && node.node_name().find(request->filter()) == std::string::npos) {
            continue; // 过滤掉不匹配的节点
        }
        *response->add_nodes() = node;
    }
    
    response->set_success(true);
    response->set_message("Get nodes list success");
    
    LOG_INFO << "GetNodes request processed, found " << response->nodes_size() << " nodes";
    
    return grpc::Status::OK;
}

// 获取节点详细信息
grpc::Status RosRpcServiceImpl::GetNodeInfo(grpc::ServerContext*,
                                           const GetNodeInfoRequest* request,
                                           GetNodeInfoResponse* response) {
    const std::string& node_name = request->node_name();
    
    // 检查节点是否存在
    if (!graph_->HasNode(node_name)) {
        response->set_success(false);
        response->set_message("Node not found: " + node_name);
        LOG_WARN << "GetNodeInfo request failed: node not found - " << node_name;
        return grpc::Status::OK; // 返回成功状态，但内容显示失败
    }
    
    // 获取节点基本信息
    NodeInfo node_info;
    graph_->GetNodeByName(node_name, &node_info);
    *response->mutable_node_info() = node_info;
    
    // 获取节点发布的话题（包含消息类型）
    auto publish_topic_keys = graph_->GetNodePublishTopicKeys(node_name);
    for (const auto& topic_key : publish_topic_keys) {
        TopicInfo* topic_info = response->add_publishes();
        topic_info->set_topic_name(topic_key.topic);
        topic_info->set_msg_type(topic_key.msg_type);  // 现在可以设置正确的消息类型了
    }
    
    // 获取节点订阅的话题（包含消息类型）
    auto subscribe_topic_keys = graph_->GetNodeSubscribeTopicKeys(node_name);
    for (const auto& topic_key : subscribe_topic_keys) {
        TopicInfo* topic_info = response->add_subscribes();
        topic_info->set_topic_name(topic_key.topic);
        topic_info->set_msg_type(topic_key.msg_type);  // 现在可以设置正确的消息类型了
    }
    
    response->set_success(true);
    response->set_message("Get node info success");
    
    LOG_INFO << "GetNodeInfo request processed for node: " << node_name;
    
    return grpc::Status::OK;
}



// ========== Server ==========
RosRpcServer::RosRpcServer(const std::string& server_address, std::shared_ptr<MasterTcpServer> tcp_server, std::shared_ptr<MessageGraph> graph)
    : server_address_(server_address), service_(tcp_server, graph) {}

RosRpcServer::~RosRpcServer() {
    Shutdown();
}

void RosRpcServer::Run() {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address_, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);
    server_ = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address_ << std::endl;
    server_->Wait();
}

void RosRpcServer::Shutdown() {
    if (server_) server_->Shutdown();
}




// ========== 新增 RPC 方法实现 ==========
// 获取话题列表
grpc::Status RosRpcServiceImpl::GetTopics(grpc::ServerContext* context,
                                         const GetTopicsRequest* request,
                                         GetTopicsResponse* response) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    try {
        // 获取所有节点的发布和订阅话题
        std::unordered_map<std::string, std::string> topic_msg_types;
        
        // 遍历所有节点
        auto all_nodes = graph_->GetAllNodes();
        for (const auto& node : all_nodes) {
            // 获取节点发布的话题（包含消息类型）
            auto publish_topics = graph_->GetNodePublishTopicKeys(node.node_name());
            for (const auto& topic_key : publish_topics) {
                topic_msg_types[topic_key.topic] = topic_key.msg_type;
            }
            
            // 获取节点订阅的话题（包含消息类型）
            auto subscribe_topics = graph_->GetNodeSubscribeTopicKeys(node.node_name());
            for (const auto& topic_key : subscribe_topics) {
                topic_msg_types[topic_key.topic] = topic_key.msg_type;
            }
        }
        
        // 应用过滤条件
        const std::string& filter = request->filter();
        for (const auto& pair : topic_msg_types) {
            const std::string& topic_name = pair.first;
            const std::string& msg_type = pair.second;
            
            // 如果没有过滤条件或话题名包含过滤字符串
            if (filter.empty() || topic_name.find(filter) != std::string::npos) {
                TopicInfo* topic_info = response->add_topics();
                topic_info->set_topic_name(topic_name);
                topic_info->set_msg_type(msg_type);
            }
        }
        
        response->set_success(true);
        response->set_message("Get topics success");
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_message(std::string("Get topics failed: ") + e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, response->message());
    }
}

// 获取话题详细信息
grpc::Status RosRpcServiceImpl::GetTopicInfo(grpc::ServerContext* context,
                                           const GetTopicInfoRequest* request,
                                           GetTopicInfoResponse* response) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    try {
        const std::string& topic_name = request->topic_name();
        
        // 查找该话题的消息类型
        std::string msg_type;
        bool topic_exists = false;
        
        // 遍历所有节点查找该话题
        auto all_nodes = graph_->GetAllNodes();
        for (const auto& node : all_nodes) {
            // 检查发布的话题
            auto publish_topics = graph_->GetNodePublishTopicKeys(node.node_name());
            for (const auto& topic_key : publish_topics) {
                if (topic_key.topic == topic_name) {
                    msg_type = topic_key.msg_type;
                    topic_exists = true;
                    break;
                }
            }
            if (topic_exists) break;
            
            // 检查订阅的话题
            auto subscribe_topics = graph_->GetNodeSubscribeTopicKeys(node.node_name());
            for (const auto& topic_key : subscribe_topics) {
                if (topic_key.topic == topic_name) {
                    msg_type = topic_key.msg_type;
                    topic_exists = true;
                    break;
                }
            }
            if (topic_exists) break;
        }
        
        if (!topic_exists) {
            response->set_success(false);
            response->set_message("Topic not found");
            return grpc::Status(grpc::StatusCode::NOT_FOUND, response->message());
        }
        
        // 获取发布该话题的节点列表
        auto publishers = graph_->GetPublishersByTopic(topic_name);
        for (const auto& publisher : publishers) {
            NodeInfo* node_info = response->add_publishers();
            *node_info = publisher;
        }
        
        // 获取订阅该话题的节点列表
        auto subscribers = graph_->GetSubscribersByTopic(topic_name);
        for (const auto& subscriber : subscribers) {
            NodeInfo* node_info = response->add_subscribers();
            *node_info = subscriber;
        }
        
        // 设置响应信息
        response->set_success(true);
        response->set_message("Get topic info success");
        response->set_topic_name(topic_name);
        response->set_msg_type(msg_type);
        
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_message(std::string("Get topic info failed: ") + e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, response->message());
    }
}


} // namespace simple_ros
