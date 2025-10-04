#include <iostream>
#include <memory>
#include <thread>
#include <grpcpp/grpcpp.h>
#include <muduo/net/EventLoop.h>
#include "ros_rpc_server.h"
#include "master_tcp_server.h"

using namespace simple_ros;

int main() {
    muduo::net::EventLoop loop;
    
    // 1. 首先创建 MessageGraph
    auto graph = std::make_shared<simple_ros::MessageGraph>();
    
    // 2. 创建MasterTcpServer，传入graph智能指针
    auto tcp_server = std::make_shared<MasterTcpServer>(&loop, graph);
    tcp_server->Start();
    
    // 3. 创建RosRpcServer，传入tcp_server和graph
    std::string server_address("0.0.0.0:50051");
    RosRpcServer server(server_address, tcp_server, graph);
    
    // 4. 在单独的线程中运行gRPC服务
    std::thread server_thread(&RosRpcServer::Run, &server);
    
    // 5. 运行Muduo事件循环
    loop.loop();
    
    // 6. 关闭服务器
    server.Shutdown();
    if (server_thread.joinable()) {
        server_thread.join();
    }
    
    return 0;
}
