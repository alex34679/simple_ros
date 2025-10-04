#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include "ros_rpc_server.h"
#include "ros_rpc_client.h"
#include "master_tcp_server.h"
#include "muduo/net/EventLoop.h"
using namespace simple_ros;
int main() {
    const std::string server_address = "0.0.0.0:50051";

    // 创建事件循环和TCP服务器
    muduo::net::EventLoop loop;
    
    // 1. 创建 MessageGraph 智能指针
    auto graph = std::make_shared<simple_ros::MessageGraph>();
    
    // 2. 创建 MasterTcpServer，传入 graph
    MasterTcpServer tcp_server(&loop, graph);
    std::shared_ptr<MasterTcpServer> tcp_server_ptr(&tcp_server, [](MasterTcpServer*){});
    tcp_server.Start();

    // 启动服务端线程
    std::thread server_thread([&]() {
        // 3. 创建 RosRpcServer，传入 tcp_server_ptr 和 graph
        simple_ros::RosRpcServer server(server_address, tcp_server_ptr, graph);
        server.Run();
    });

    // 等待服务端启动
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 创建客户端
    simple_ros::RosRpcClient client(server_address);

    // -------------------
    // 1. 注册发布者
    // -------------------
    simple_ros::NodeInfo node_info_pub;
    node_info_pub.set_ip("127.0.0.1");
    node_info_pub.set_port(50052);
    node_info_pub.set_node_name("publisher_node");

    simple_ros::RegisterPublisherResponse register_response;
    bool register_ok = client.RegisterPublisher("chatter", "std_msgs/String", node_info_pub, &register_response);
    if (register_ok) {
        std::cout << "[Client] RegisterPublisher: " << register_response.message() << std::endl;
    } else {
        std::cerr << "[Client] RegisterPublisher failed" << std::endl;
    }

    // -------------------
    // 2. 订阅话题
    // -------------------
    simple_ros::NodeInfo node_info_sub;
    node_info_sub.set_ip("127.0.0.1");
    node_info_sub.set_port(50053);
    node_info_sub.set_node_name("subscriber_node");

    simple_ros::SubscribeResponse subscribe_response;
    bool subscribe_ok = client.Subscribe("chatter", "std_msgs/String", node_info_sub, &subscribe_response);
    if (subscribe_ok) {
        std::cout << "[Client] Subscribe: " << subscribe_response.message() << std::endl;
        for (const auto& node : subscribe_response.node_info()) {
            std::cout << "  Node: " << node.node_name()
                      << " IP: " << node.ip()
                      << " Port: " << node.port() << std::endl;
        }
    } else {
        std::cerr << "[Client] Subscribe failed" << std::endl;
    }

    // -------------------
    // 3. 解除订阅
    // -------------------
    simple_ros::UnsubscribeResponse unsub_response;
    bool unsub_ok = client.Unsubscribe("chatter", "std_msgs/String", node_info_sub, &unsub_response);
    if (unsub_ok) {
        std::cout << "[Client] Unsubscribe: " << unsub_response.message() << std::endl;
    } else {
        std::cerr << "[Client] Unsubscribe failed" << std::endl;
    }

    // -------------------
    // 4. 解除发布
    // -------------------
    simple_ros::UnregisterPublisherResponse unreg_response;
    bool unreg_ok = client.UnregisterPublisher("chatter", "std_msgs/String", node_info_pub, &unreg_response);
    if (unreg_ok) {
        std::cout << "[Client] UnregisterPublisher: " << unreg_response.message() << std::endl;
    } else {
        std::cerr << "[Client] UnregisterPublisher failed" << std::endl;
    }

    // -------------------
    // 5. 停止服务端
    // -------------------
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::exit(0);
}
