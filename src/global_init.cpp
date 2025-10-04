#include "global_init.h"
#include <muduo/base/Logging.h>
#include <chrono>
#include <thread>
#include "ros_rpc_client.h"  // 添加ROS RPC客户端头文件

using namespace simple_ros;

SystemManager& SystemManager::instance() {
    static SystemManager inst;
    return inst;
}

SystemManager::SystemManager() = default;

SystemManager::~SystemManager() {
    shutdown();  // 程序退出时自动清理
}

void SystemManager::init(int port, std::string node_name) {
    // 初始化节点信息
    nodeInfo_.set_node_name(node_name);
    nodeInfo_.set_ip("127.0.0.1");  // 默认本地IP
    nodeInfo_.set_port(port);
    
    LOG_INFO << "NodeInfo initialized: name= " << node_name << ", port= " << port;
    
    // 调用现有init(port)方法继续初始化
    init(port);
}

void SystemManager::init(const std::string& node_name) {
    int port = findAvailablePort();
    if (port < 0) {
        LOG_ERROR << "No available port found in range 60000-61000";
        throw std::runtime_error("No available port for node initialization");
    }

    // 调用原来的手动端口版本
    init(port, node_name);
}

void SystemManager::init(int port) {
    if (!messageQueue_) {
        messageQueue_ = std::make_shared<MessageQueue>();
    }

    // 创建全局RPC客户端，连接到主服务器
    rpcClient_ = std::make_shared<RosRpcClient>("localhost:50051");
    LOG_INFO << "Global RosRpcClient initialized";

    // 启动后台线程
    eventThread_ = std::thread([this, port]() {  // <-- 显式捕获 port
        eventLoop_ = std::make_shared<muduo::net::EventLoop>();

        muduo::net::InetAddress listenAddr("127.0.0.1", port);
        pollManager_ = std::make_shared<PollManager>(eventLoop_.get(), listenAddr);

        pollManager_->start();
        LOG_INFO << "PollManager started in background thread";

        eventLoop_->loop();  // 阻塞直到 quit()

        pollManager_.reset();
        eventLoop_.reset();
    });
}


void SystemManager::init() {
    init(12345);
}

void SystemManager::spin() {
    while (running_) {
        if (messageQueue_) {
            messageQueue_->processCallbacks();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void SystemManager::spinOnce() {
    if (messageQueue_) {
        messageQueue_->processCallbacks();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void SystemManager::shutdown() {
    running_ = false;
    // 1. 安全退出 EventLoop
    if (eventLoop_) {
        eventLoop_->runInLoop([this]() { eventLoop_->quit(); });
    }
    // 2. 等待后台线程退出
    if (eventThread_.joinable()) {
        eventThread_.join();
    }
    messageQueue_.reset();
    LOG_INFO << "SystemManager shutdown complete";
}


int SystemManager::findAvailablePort(int start_port, int end_port) {
    for (int port = start_port; port <= end_port; ++port) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        int bind_result = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
        close(sock);

        if (bind_result == 0) {
            return port;  // 找到可用端口
        }
    }
    return -1; // 没有可用端口
}
