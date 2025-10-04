#include "subscriber.h"
#include "message_queue.h"
#include "global_init.h"
#include "msg_factory.h"

template<typename MsgType>
Subscriber::Subscriber(const std::string& topic,
                       uint32_t queue_size,
                       std::function<void(const std::shared_ptr<MsgType>&)> typed_callback)
    : topic_(topic), queue_size_(queue_size)
{
    // 类型擦除回调，直接使用 MsgType 指针，无需 dynamic_pointer_cast
    callback_ = [typed_callback](const std::shared_ptr<google::protobuf::Message>& msg_base) {
        // 创建 MsgType 实例
        auto typed_msg = std::make_shared<MsgType>();
        
        // 将收到的 Message 数据序列化后再解析到具体类型
        std::string serialized;
        msg_base->SerializeToString(&serialized);
        if (!typed_msg->ParseFromString(serialized)) {
            LOG_ERROR << "Failed to parse message to " << MsgType::descriptor()->full_name();
            return;
        }

        typed_callback(typed_msg);
    };


    // 获取消息队列
    auto msg_queue = SystemManager::instance().getMessageQueue();
    if (!msg_queue) {
        LOG_ERROR << "MessageQueue not initialized when creating Subscriber for topic: " << topic;
        return;
    }

    msg_queue_ = msg_queue;

    // 注册 Topic
    msg_queue->registerTopic(topic);
    msg_queue->setTopicMaxQueueSize(topic, queue_size);

    // 添加订阅者回调
    msg_queue->addSubscriber(topic, callback_);

    LOG_INFO << "Subscriber created for topic: " << topic
             << ", message type: " << MsgType::descriptor()->full_name();
}
