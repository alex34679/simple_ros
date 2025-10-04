#pragma once
#include <functional>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <string>
#include <google/protobuf/message.h>
#include <muduo/base/Logging.h>


class MessageQueue {
public:
    using Callback = std::function<void(const std::shared_ptr<google::protobuf::Message>&)>;

    // 构造函数，可以设置默认队列大小
    MessageQueue(uint32_t default_max_queue_size = 1000) 
        : default_max_queue_size_(default_max_queue_size) {}

    // 设置主题的最大队列大小
    void setTopicMaxQueueSize(const std::string& topic, uint32_t max_size) {
        std::lock_guard<std::mutex> lock(mutex_);
        topic_max_queue_sizes_[topic] = max_size;
    }

    // 添加订阅者
    void addSubscriber(const std::string& topic, Callback cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_[topic].push_back(std::move(cb));
    }

    // 移除订阅者
// 移除整个 topic 的所有订阅者
    void removeSubscriber(const std::string& topic) {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_.erase(topic);
        message_queues_.erase(topic);
        topic_max_queue_sizes_.erase(topic);
        registered_topics_.erase(topic);
        LOG_INFO << "Removed topic and all its subscribers: " << topic;
    }


    // 注册主题
    void registerTopic(const std::string& topic) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (registered_topics_.find(topic) == registered_topics_.end()) {
            registered_topics_.insert(topic);
            LOG_INFO << "Topic registered: " << topic;
        }
    }

    void push(const std::string& topic, std::shared_ptr<google::protobuf::Message> msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (registered_topics_.find(topic) == registered_topics_.end()) {
            LOG_WARN << "Received message for unregistered topic: " << topic;
            return;
        }

        // 检查队列是否已满
        auto it = topic_max_queue_sizes_.find(topic);
        uint32_t max_size = (it != topic_max_queue_sizes_.end()) ? it->second : default_max_queue_size_;

        // 队列已满，移除最早的消息
        if (message_queues_[topic].size() >= max_size) {
            message_queues_[topic].pop_front();
        }

        // 添加新消息到队列
        message_queues_[topic].push_back(msg);
    }

    void processCallbacks() {
        std::lock_guard<std::mutex> lock(mutex_);

        // 遍历所有主题的消息队列
        for (auto& topic_entry : message_queues_) {
            const std::string& topic = topic_entry.first;
            auto& queue = topic_entry.second;

            // 如果队列不为空
            if (!queue.empty()) {
                // 获取队首消息
                auto msg = queue.front();
                // 移除处理后的消息
                queue.pop_front();

                // 查找该主题的订阅者
                auto sub_it = subscribers_.find(topic);
                if (sub_it != subscribers_.end()) {
                    // 通知所有订阅者处理消息
                    for (const auto& callback : sub_it->second) {
                        callback(msg);
                    }
                }

                // 只处理一个消息就返回
                return;
            }
        }
    }

private:
    uint32_t default_max_queue_size_;                            // 默认队列大小
    std::unordered_set<std::string> registered_topics_;          // 已注册的主题
    std::unordered_map<std::string, uint32_t> topic_max_queue_sizes_; // 主题最大队列大小
    std::unordered_map<std::string, std::list<std::shared_ptr<google::protobuf::Message>>> message_queues_; // 消息队列
    std::unordered_map<std::string, std::vector<Callback>> subscribers_; // 订阅者
    std::mutex mutex_; // 互斥锁
};