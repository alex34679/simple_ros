#include <string>
#include <memory>
#include <iostream>
#include <chrono>
#include <deque>
#include <grpcpp/grpcpp.h>
#include "ros_rpc.grpc.pb.h"
#include "ros_rpc_client.h"
#include "global_init.h"
#include "node_handle.h"
#include "msg_factory.h"
#include <unordered_map>
#include <functional>
#include <any>
#include <typeindex>
#include "subscription_handler_registry.h"

#include "example.pb.h"
#include "marker.pb.h"

void usage() {
    std::cout << "Usage:\n";
    std::cout << "  rostopic list                 List all active topics\n";
    std::cout << "  rostopic info <topic>         Print information about a topic\n";
    std::cout << "  rostopic echo <topic>         Print messages published to a topic\n";
    std::cout << "  rostopic hz <topic> [window]  Print message publishing rate\n";
    std::cout << "                                 window: number of samples to average (default 100)\n";
}

// 优化后的echo命令实现
void echoTopic(const std::string& topic_name, simple_ros::RosRpcClient& client) {
    simple_ros::GetTopicInfoResponse topic_info_response;
    if (!client.GetTopicInfo(topic_name, &topic_info_response)) {
        std::cerr << "Failed to get topic info: " << topic_info_response.message() << std::endl;
        return;
    }

    if (!topic_info_response.success()) {
        std::cerr << "Error: " << topic_info_response.message() << std::endl;
        return;
    }

    std::string msg_type = topic_info_response.msg_type();
    std::cout << "Subscribing to topic: " << topic_name << " with message type: " << msg_type << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl;

    try {
        SystemManager::instance().init(60002, "rostopic_echo_node");
        NodeHandle nh;
        auto& registry = SubscriptionHandlerRegistry::getInstance();
        std::shared_ptr<Subscriber> subscriber = registry.createSubscription(nh, topic_name, msg_type);
        if (!subscriber) {
            std::cerr << "Failed to subscribe to topic: " << topic_name << std::endl;
            return;
        }
        SystemManager::instance().spin();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// 新增hz命令实现
void hzTopic(const std::string& topic_name, simple_ros::RosRpcClient& client, size_t window = 100) {
    simple_ros::GetTopicInfoResponse topic_info_response;
    if (!client.GetTopicInfo(topic_name, &topic_info_response)) {
        std::cerr << "Failed to get topic info: " << topic_info_response.message() << std::endl;
        return;
    }

    if (!topic_info_response.success()) {
        std::cerr << "Error: " << topic_info_response.message() << std::endl;
        return;
    }

    std::string msg_type = topic_info_response.msg_type();
    std::cout << "Measuring publishing rate for topic: " << topic_name 
              << " with message type: " << msg_type << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl;

    try {
        SystemManager::instance().init(60002, "rostopic_hz_node");
        NodeHandle nh;

        // 用于存储时间戳
        auto timestamps = std::make_shared<std::deque<std::chrono::steady_clock::time_point>>();

        // 使用带回调的createSubscription接口
        auto subscriber = SubscriptionHandlerRegistry::getInstance().createSubscription(
            nh,
            topic_name,
            msg_type,
            [timestamps, window](const std::shared_ptr<google::protobuf::Message>& msg) {
                auto now = std::chrono::steady_clock::now();
                timestamps->push_back(now);
                if (timestamps->size() > window) {
                    timestamps->pop_front();
                }
            }
        );

        if (!subscriber) {
            std::cerr << "Failed to subscribe to topic: " << topic_name << std::endl;
            return;
        }

        // 定时器线程，用于周期性打印频率
        std::thread print_thread([timestamps]() {
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (timestamps->size() > 1) {
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        timestamps->back() - timestamps->front()
                    ).count();
                    double rate = 1000.0 * (timestamps->size() - 1) / duration;
                    std::cout << "\rAverage rate (" << timestamps->size() 
                              << " samples): " << rate << " Hz   " << std::flush;
                }
            }
        });

        // 事件循环
        SystemManager::instance().spin();

        // 如果spin退出，结束打印线程
        print_thread.join();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}



int main(int argc, char** argv) {
    const std::string server_address = "localhost:50051";
    simple_ros::RosRpcClient client(server_address);

    if (argc < 2) {
        usage();
        return 1;
    }

    std::string command = argv[1];

    if (command == "list") {
        simple_ros::GetTopicsResponse response;
        if (!client.GetTopics("", &response)) {
            std::cerr << "Failed to get topics list" << std::endl;
            return 1;
        }
        if (!response.success()) {
            std::cerr << "Error: " << response.message() << std::endl;
            return 1;
        }

        std::cout << "Active topics:\n";
        for (const auto& topic : response.topics()) {
            std::cout << " * " << topic.topic_name() << " [" << topic.msg_type() << "]" << std::endl;
        }

    } else if (command == "info") {
        if (argc < 3) {
            std::cerr << "Error: Missing topic name\n";
            usage();
            return 1;
        }
        std::string topic_name = argv[2];
        simple_ros::GetTopicInfoResponse topic_info_response;
        if (!client.GetTopicInfo(topic_name, &topic_info_response)) {
            std::cerr << "Failed to get topic info" << std::endl;
            return 1;
        }
        if (!topic_info_response.success()) {
            std::cerr << "Error: " << topic_info_response.message() << std::endl;
            return 1;
        }

        std::cout << "Topic: " << topic_name << "\nType: " << topic_info_response.msg_type() << std::endl;
        std::cout << "Publishers: " << (topic_info_response.publishers_size() ? "" : "None") << std::endl;
        for (const auto& node : topic_info_response.publishers()) {
            std::cout << "  * " << node.node_name() << " (" << node.ip() << ":" << node.port() << ")" << std::endl;
        }
        std::cout << "Subscribers: " << (topic_info_response.subscribers_size() ? "" : "None") << std::endl;
        for (const auto& node : topic_info_response.subscribers()) {
            std::cout << "  * " << node.node_name() << " (" << node.ip() << ":" << node.port() << ")" << std::endl;
        }

    } else if (command == "echo") {
        if (argc < 3) {
            std::cerr << "Error: Missing topic name\n";
            usage();
            return 1;
        }
        std::string topic_name = argv[2];
        echoTopic(topic_name, client);

    } else if (command == "hz") {
        if (argc < 3) {
            std::cerr << "Error: Missing topic name\n";
            usage();
            return 1;
        }
        std::string topic_name = argv[2];
        size_t window = 100;
        if (argc >= 4) {
            window = std::stoul(argv[3]);
        }
        hzTopic(topic_name, client, window);

    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        usage();
        return 1;
    }

    return 0;
}
