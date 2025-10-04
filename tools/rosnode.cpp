#include <iostream>
#include <string>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "ros_rpc.grpc.pb.h"
#include "ros_rpc_client.h"

void usage() {
    std::cout << "Usage:\n";
    std::cout << "  rosnode list            List all active nodes\n";
    std::cout << "  rosnode info <node>     Print information about a node\n";
}

int main(int argc, char** argv) {
    // 默认连接到本地50051端口的RPC服务器
    const std::string server_address = "localhost:50051";
    
    // 创建RPC客户端
    simple_ros::RosRpcClient client(server_address);
    
    if (argc < 2) {
        usage();
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "list") {
        // 执行rosnode list命令
        simple_ros::GetNodesResponse response;
        if (!client.GetNodes("", &response)) {
            std::cerr << "Failed to get nodes list\n";
            return 1;
        }
        
        if (!response.success()) {
            std::cerr << "Error: " << response.message() << std::endl;
            return 1;
        }
        
        std::cout << "Active nodes:\n";
        for (const auto& node : response.nodes()) {
            std::cout << " * " << node.node_name() << " (" << node.ip() << ":" << node.port() << ")" << std::endl;
        }
        std::cout << "Total nodes: " << response.nodes_size() << std::endl;
        
    } else if (command == "info") {
        // 执行rosnode info命令
        if (argc < 3) {
            std::cerr << "Error: Missing node name\n";
            usage();
            return 1;
        }
        
        std::string node_name = argv[2];
        simple_ros::GetNodeInfoResponse response;
        if (!client.GetNodeInfo(node_name, &response)) {
            std::cerr << "Failed to get node info\n";
            return 1;
        }
        
        if (!response.success()) {
            std::cerr << "Error: " << response.message() << std::endl;
            return 1;
        }
        
        // 打印节点详细信息
        std::cout << "Node: " << response.node_info().node_name() << std::endl;
        std::cout << " - IP: " << response.node_info().ip() << std::endl;
        std::cout << " - Port: " << response.node_info().port() << std::endl;
        
        std::cout << "Published topics: " << std::endl;
        if (response.publishes_size() == 0) {
            std::cout << "  None" << std::endl;
        } else {
            for (const auto& topic : response.publishes()) {
                std::cout << "  * " << topic.topic_name() << " (" << topic.msg_type() << ")" << std::endl;
            }
        }
        
        std::cout << "Subscribed topics: " << std::endl;
        if (response.subscribes_size() == 0) {
            std::cout << "  None" << std::endl;
        } else {
            for (const auto& topic : response.subscribes()) {
                std::cout << "  * " << topic.topic_name() << " (" << topic.msg_type() << ")" << std::endl;
            }
        }
        
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        usage();
        return 1;
    }
    
    return 0;
}