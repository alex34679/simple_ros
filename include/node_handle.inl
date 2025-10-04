#include "node_handle.h"
#include "message_queue.h"
#include "global_init.h"
#include <muduo/base/Logging.h>

using namespace simple_ros;
// 模板方法实现

template<typename MsgType>
std::shared_ptr<Subscriber> NodeHandle::subscribe(const std::string& topic, 
                                                uint32_t queue_size, 
                                                std::function<void(const std::shared_ptr<MsgType>&)> callback) {
    // 获取消息类型名称
    std::string msg_type_name = MsgType::descriptor()->full_name();
    LOG_INFO << "Subscribe to topic=" << topic << ", type=" << msg_type_name;

    // 创建订阅者实例
    auto subscriber = std::make_shared<Subscriber>(topic, queue_size, callback);

    // 调用RPC订阅
    auto rpc_client = SystemManager::instance().getRpcClient();
    if (rpc_client) {
        SubscribeResponse response;
        bool success = rpc_client->Subscribe(topic, msg_type_name, nodeInfo_, &response);
        if (success) {
            LOG_INFO << "Subscribe RPC successful for topic: " << topic;
        } else {
            LOG_ERROR << "Subscribe RPC failed for topic: " << topic;
        }
    } else {
        LOG_ERROR << "Global RPC client not initialized";
    }

    return subscriber;
}

template<typename MsgType, typename Class>
std::shared_ptr<Subscriber> NodeHandle::subscribe(const std::string& topic, 
                                                uint32_t queue_size, 
                                                void(Class::*callback)(const std::shared_ptr<MsgType>&), 
                                                Class* instance) {
    // 创建一个包装类成员函数的lambda表达式
    auto wrapped_callback = [callback, instance](const std::shared_ptr<MsgType>& msg) {
        (instance->*callback)(msg);
    };
    std::string msg_type_name = MsgType::descriptor()->full_name();
    LOG_INFO << "Subscribe to topic=" << topic << ", type=" << msg_type_name;

    // 创建订阅者实例
    auto subscriber = std::make_shared<Subscriber>(topic, queue_size, wrapped_callback);

    // 调用RPC订阅
    auto rpc_client = SystemManager::instance().getRpcClient();
    if (rpc_client) {
        SubscribeResponse response;
        bool success = rpc_client->Subscribe(topic, msg_type_name, nodeInfo_, &response);
        if (success) {
            LOG_INFO << "Subscribe RPC successful for topic: " << topic;
        } else {
            LOG_ERROR << "Subscribe RPC failed for topic: " << topic;
        }
    } else {
        LOG_ERROR << "Global RPC client not initialized";
    }

    return subscriber;
}

template<typename MsgType>
std::shared_ptr<Publisher<MsgType>> NodeHandle::advertise(const std::string& topic)
{
    // 获取消息类型名称
    std::string msg_type_name = MsgType::descriptor()->full_name();
    LOG_INFO << "Advertise topic=" << topic << ", type=" << msg_type_name;

    // 创建发布者实例
    auto publisher = std::make_shared<Publisher<MsgType>>(topic);
    LOG_INFO << "Debug: nodeInfo_ details - node_name: '" << nodeInfo_.node_name() 
             << "', ip: '" << nodeInfo_.ip() 
             << "', port: " << nodeInfo_.port();
    // 调用RPC注册发布者
    auto rpc_client = SystemManager::instance().getRpcClient();
    if (rpc_client) {
        RegisterPublisherResponse response;
        bool success = rpc_client->RegisterPublisher(topic, msg_type_name, nodeInfo_, &response);
        if (success) {
            LOG_INFO << "RegisterPublisher RPC successful for topic: " << topic;
        } else {
            LOG_ERROR << "RegisterPublisher RPC failed for topic: " << topic;
        }
    } else {
        LOG_ERROR << "Global RPC client not initialized";
    }

    return publisher;
}
