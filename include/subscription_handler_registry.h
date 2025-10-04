#pragma once

#include <functional>
#include <memory>
#include <iostream>
#include <string>

#include "node_handle.h"   // NodeHandle & Subscriber
#include "msg_factory.h"   // MsgFactory
#include "example.pb.h"
#include "marker.pb.h"

typedef std::function<void(const std::shared_ptr<google::protobuf::Message>&)> GenericMessageCallback;

class SubscriptionHandlerRegistry {
public:
    static SubscriptionHandlerRegistry& getInstance();

    template<typename MsgType>
    void registerHandler();

    // 创建订阅（带默认回调）
    std::shared_ptr<Subscriber> createSubscription(NodeHandle& nh,
                                                   const std::string& topic_name,
                                                   const std::string& msg_type_name);

    // 创建订阅（可传回调）
    std::shared_ptr<Subscriber> createSubscription(NodeHandle& nh,
                                                   const std::string& topic_name,
                                                   const std::string& msg_type_name,
                                                   GenericMessageCallback callback);

private:
    SubscriptionHandlerRegistry(); // 私有构造函数

    // 禁止拷贝和移动
    SubscriptionHandlerRegistry(const SubscriptionHandlerRegistry&) = delete;
    SubscriptionHandlerRegistry& operator=(const SubscriptionHandlerRegistry&) = delete;
    SubscriptionHandlerRegistry(SubscriptionHandlerRegistry&&) = delete;
    SubscriptionHandlerRegistry& operator=(SubscriptionHandlerRegistry&&) = delete;
};
