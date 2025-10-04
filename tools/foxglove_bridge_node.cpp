#include "foxglove_bridge.h"

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <sstream>

#include "ros_rpc_client.h" // 使用 RPC 直接查询话题

using namespace std::chrono_literals;

// 全局变量用于信号处理
std::atomic_bool g_running(true);
std::unique_ptr<FoxgloveBridge> g_bridge;

// 打印使用说明
void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n"
              << "Options:\n"
              << "  -p, --port PORT        Foxglove WebSocket server port (default: 8765)\n"
              << "  --host HOST            Foxglove WebSocket server host (default: 127.0.0.1)\n"
              << "  -r, --ros-port PORT    RPC server port (default: 12347)\n"
              << "  -n, --node-name NAME   ROS node name (unused in new bridge) (default: foxglove_bridge)\n"
              << "  -t, --topics TOPICS    Comma-separated list of topics to subscribe (topic:type) [Note: manual subscribe not supported in this bridge build]\n"
              << "  -q, --queue-size SIZE  Queue size for topic subscriptions (default: 10)\n"
              << "  -l, --list-topics      List available topics and exit\n"
              << "  --no-auto-discovery    Disable automatic topic discovery (NOT supported in current FoxgloveBridge)\n"
              << "  --discovery-interval MS Discovery interval in milliseconds (ignored; bridge handles discovery internally)\n"
              << "  --help                 Show this help message\n"
              << "\n";
}

// 解析命令行参数
struct BridgeConfig {
    int foxglove_port = 8765;
    std::string foxglove_host = "127.0.0.1";
    int ros_port = 50051; // 用于构造 rpc address localhost:ros_port
    std::string node_name = "foxglove_bridge";
    std::vector<std::pair<std::string, std::string>> topics;
    uint32_t queue_size = 10;
    bool list_topics = false;
    bool auto_discovery = true;  // 默认启用自动发现
    int discovery_interval = 1000;  // 5秒间隔 (仅为兼容选项)
};

BridgeConfig parseArguments(int argc, char* argv[]) {
    BridgeConfig config;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            exit(0);
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) config.foxglove_port = std::stoi(argv[++i]);
            else { std::cerr << "Error: -p/--port requires a port number\n"; exit(1); }
        } else if (arg == "--host") {
            if (i + 1 < argc) config.foxglove_host = argv[++i];
            else { std::cerr << "Error: --host requires a host address\n"; exit(1); }
        } else if (arg == "-r" || arg == "--ros-port") {
            if (i + 1 < argc) config.ros_port = std::stoi(argv[++i]);
            else { std::cerr << "Error: -r/--ros-port requires a port number\n"; exit(1); }
        } else if (arg == "-n" || arg == "--node-name") {
            if (i + 1 < argc) config.node_name = argv[++i];
            else { std::cerr << "Error: -n/--node-name requires a node name\n"; exit(1); }
        } else if (arg == "-t" || arg == "--topics") {
            if (i + 1 < argc) {
                std::string topics_str = argv[++i];
                std::stringstream ss(topics_str);
                std::string topic_pair;
                while (std::getline(ss, topic_pair, ',')) {
                    size_t colon_pos = topic_pair.find(':');
                    if (colon_pos != std::string::npos) {
                        std::string topic_name = topic_pair.substr(0, colon_pos);
                        std::string msg_type = topic_pair.substr(colon_pos + 1);
                        config.topics.emplace_back(topic_name, msg_type);
                    } else {
                        std::cerr << "Error: Invalid topic format: " << topic_pair
                                  << ". Expected format: topic_name:message_type\n";
                        exit(1);
                    }
                }
            } else { std::cerr << "Error: -t/--topics requires a comma-separated list\n"; exit(1); }
        } else if (arg == "-q" || arg == "--queue-size") {
            if (i + 1 < argc) config.queue_size = std::stoul(argv[++i]);
            else { std::cerr << "Error: -q/--queue-size requires a number\n"; exit(1); }
        } else if (arg == "-l" || arg == "--list-topics") {
            config.list_topics = true;
        } else if (arg == "--no-auto-discovery") {
            config.auto_discovery = false;
        } else if (arg == "--discovery-interval") {
            if (i + 1 < argc) config.discovery_interval = std::stoi(argv[++i]);
            else { std::cerr << "Error: --discovery-interval requires a number\n"; exit(1); }
        } else {
            std::cerr << "Error: Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            exit(1);
        }
    }
    return config;
}

// 列出可用话题（直接用 RPC）
void listTopics(const std::string& rpc_address) {
    try {
        simple_ros::RosRpcClient client(rpc_address);
        simple_ros::GetTopicsResponse response;
        if (!client.GetTopics("", &response)) {
            std::cerr << "Failed to get topics from RPC server\n";
            return;
        }
        if (!response.success()) {
            std::cerr << "Error: " << response.message() << std::endl;
            return;
        }
        std::cout << "Available topics:\n";
        if (response.topics_size() == 0) {
            std::cout << "  No active topics found\n";
            return;
        }
        for (const auto& topic : response.topics()) {
            std::cout << "  " << topic.topic_name() << " [" << topic.msg_type() << "]\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error listing topics: " << e.what() << std::endl;
    }
}

// 显示状态信息
void showStatusInfo() {
    std::cout << "Foxglove Bridge is running!\n"
              << " - Visualizations: publish visualization_msgs::Marker/MarkerArray to create visuals\n"
              << " - Other messages are bridged to Foxglove as JSON (if schema available)\n\n";
}

int main(int argc, char* argv[]) {


    // 解析参数
    BridgeConfig config = parseArguments(argc, argv);

    // 构造 RPC 地址（假设 RPC server 在 localhost:<ros_port>）
    std::string rpc_address = "localhost:" + std::to_string(config.ros_port);

    if (config.list_topics) {
        listTopics(rpc_address);
        return 0;
    }

    std::cout << "Foxglove Bridge Starting...\n";
    std::cout << " Foxglove server: " << config.foxglove_host << ":" << config.foxglove_port << "\n";
    std::cout << " RPC server: " << rpc_address << "\n";

    // 创建桥接器（构造参数: rpc_address, foxglove host, foxglove port）
    g_bridge = std::make_unique<FoxgloveBridge>(rpc_address, config.foxglove_host, static_cast<uint16_t>(config.foxglove_port));

    // 初始化并启动桥接器
    if (!g_bridge->init()) {
        std::cerr << "Failed to initialize Foxglove Bridge\n";
        return 1;
    }

    if (!config.auto_discovery) {
        std::cout << "Warning: --no-auto-discovery was requested, but this build of FoxgloveBridge\n"
                  << "does not expose a manual subscription API. The bridge will still start, but\n"
                  << "no topics will be bridged unless automatic discovery is enabled in the bridge.\n";
    } else {
        std::cout << "Auto-discovery enabled (bridge handles discovery internally)\n";
    }

    if (!g_bridge->start()) {
        std::cerr << "Failed to start Foxglove Bridge\n";
        return 1;
    }

    std::cout << "Open Foxglove Studio at: http://" << config.foxglove_host << ":" << config.foxglove_port << "\n";
    std::cout << "Press Ctrl+C to stop...\n";

    // 显示一些信息
    std::this_thread::sleep_for(1s);
    showStatusInfo();

    // 主循环：每 5 秒打印一次通过 RPC 查询到的活跃话题数量（不依赖于桥的内部接口）
    int counter = 0;
    while (g_running) {
        std::this_thread::sleep_for(100ms);
        if (++counter % 50 == 0) { // ~5s
            try {
                simple_ros::RosRpcClient client(rpc_address);
                simple_ros::GetTopicsResponse response;
                if (client.GetTopics("", &response) && response.success()) {
                    std::cout << "Active topics: " << response.topics_size();
                    if (config.auto_discovery) std::cout << " (auto-discovery enabled)";
                    std::cout << "\n";
                } else {
                    std::cout << "Active topics: (failed to query RPC)\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "Error querying active topics: " << e.what() << "\n";
            }
        }
    }

    std::cout << "Shutting down Foxglove Bridge...\n";
    if (g_bridge) g_bridge->stop();

    std::cout << "Foxglove Bridge stopped successfully.\n";
    return 0;
}

