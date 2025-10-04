#include "subscriber.h"
#include "global_init.h"
#include "message_queue.h"

Subscriber::Subscriber(const std::string& topic, 
                       uint32_t queue_size, 
                       MessageQueue::Callback callback)
    : topic_(topic), queue_size_(queue_size), callback_(std::move(callback)) {
    // 通过SystemManager获取消息队列并订阅
    auto msg_queue = SystemManager::instance().getMessageQueue();
    if (msg_queue) {
        msg_queue_ = msg_queue;
        msg_queue->registerTopic(topic);
        msg_queue->setTopicMaxQueueSize(topic, queue_size);
        msg_queue->addSubscriber(topic, callback_);
    } else {
        LOG_ERROR << "MessageQueue not initialized when creating Subscriber for topic: " << topic;
    }
}

Subscriber::~Subscriber() {
    // 取消订阅
    auto msg_queue = msg_queue_.lock();
    if (msg_queue) {
        msg_queue->removeSubscriber(topic_);
        LOG_INFO << "Unsubscribed from topic: " << topic_;
    }
}
