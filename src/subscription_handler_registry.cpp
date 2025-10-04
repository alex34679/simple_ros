#include "subscription_handler_registry.h"

SubscriptionHandlerRegistry& SubscriptionHandlerRegistry::getInstance() {
    static SubscriptionHandlerRegistry instance;
    return instance;
}

SubscriptionHandlerRegistry::SubscriptionHandlerRegistry() {
    // 注册 example::SensorData 类型
    registerHandler<example::SensorData>();
    registerHandler<visualization_msgs::Marker>();
    registerHandler<visualization_msgs::MarkerArray>();
    registerHandler<geometry_msgs::Odometry>();
    registerHandler<geometry_msgs::Point>();
    
}

// 模板函数在头文件里实现
template<typename MsgType>
void SubscriptionHandlerRegistry::registerHandler() {
    MsgFactory::instance().registerMessage<MsgType>();
}

std::shared_ptr<Subscriber> SubscriptionHandlerRegistry::createSubscription(
    NodeHandle& nh,
    const std::string& topic_name,
    const std::string& msg_type_name)
{
    try {
        auto callback = [](const std::shared_ptr<google::protobuf::Message>& msg) {
            std::cout << "[" << msg->GetTypeName() << "]\n" << msg->DebugString() << std::endl;
        };
        return nh.subscribe(topic_name, 10, msg_type_name, callback);
    } catch (const std::exception& e) {
        std::cerr << "Error creating subscription: " << e.what() << std::endl;
        return nullptr;
    }
}

std::shared_ptr<Subscriber> SubscriptionHandlerRegistry::createSubscription(
    NodeHandle& nh,
    const std::string& topic_name,
    const std::string& msg_type_name,
    GenericMessageCallback callback)
{
    try {
        if (!callback) {
            callback = [](const std::shared_ptr<google::protobuf::Message>& msg) {
                std::cout << "[" << msg->GetTypeName() << "]\n" << msg->DebugString() << std::endl;
            };
        }
        return nh.subscribe(topic_name, 10, msg_type_name, callback);
    } catch (const std::exception& e) {
        std::cerr << "Error creating subscription: " << e.what() << std::endl;
        return nullptr;
    }
}
