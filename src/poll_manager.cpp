#include "poll_manager.h"
#include "msg_factory.h"
#include "global_init.h" // 访问 g_messageQueue
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/reflection.h>
#include <arpa/inet.h>
#include <muduo/base/Logging.h>

using namespace muduo;
using namespace muduo::net;

PollManager::PollManager(EventLoop* loop, const InetAddress& listenAddr)
    : server_(loop, listenAddr, "PollManager")
{
    server_.setConnectionCallback(
        std::bind(&PollManager::onConnection, this, std::placeholders::_1)
    );
    server_.setMessageCallback(
        std::bind(&PollManager::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
    );
}

void PollManager::start() {
    server_.start();
}

void PollManager::onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        LOG_INFO << "New connection: " << conn->peerAddress().toIpPort();
    } else {
        LOG_INFO << "Connection closed: " << conn->name();
    }
}

// 协议:topic_name_len(2B) + topic_name +  msg_name_len(2B) + msg_name  + msg_data_len(4B) + msg_data
void PollManager::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp) {
    while (buf->readableBytes() >= 2) { // 至少需要有topic_len(2B)
        // 读取topic长度
        uint16_t topic_len;
        memcpy(&topic_len, buf->peek(), 2);
        topic_len = ntohs(topic_len);

        // 检查是否有足够的数据读取topic
        if (buf->readableBytes() < 2 + topic_len) break;

        // 读取topic
        std::string topic(buf->peek() + 2, topic_len);

        // 检查是否有足够的数据读取msg_name_len
        if (buf->readableBytes() < 2 + topic_len + 2) break;

        // 读取消息名称长度
        uint16_t msg_name_len;
        memcpy(&msg_name_len, buf->peek() + 2 + topic_len, 2);
        msg_name_len = ntohs(msg_name_len);

        // 检查是否有足够的数据读取msg_name
        if (buf->readableBytes() < 2 + topic_len + 2 + msg_name_len) break;

        // 读取消息名称
        std::string msg_name(buf->peek() + 2 + topic_len + 2, msg_name_len);

        // 检查是否有足够的数据读取msg_len
        if (buf->readableBytes() < 2 + topic_len + 2 + msg_name_len + 4) break;

        // 读取消息数据长度
        uint32_t msg_len;
        memcpy(&msg_len, buf->peek() + 2 + topic_len + 2 + msg_name_len, 4);
        msg_len = ntohl(msg_len);

        // 检查是否有足够的数据读取msg_data
        if (buf->readableBytes() < 2 + topic_len + 2 + msg_name_len + 4 + msg_len) break;

        // 读取消息数据
        std::string msg_data(buf->peek() + 2 + topic_len + 2 + msg_name_len + 4, msg_len);

        // 处理消息
        handleMessage(topic, msg_name, msg_data);

        // 移动缓冲区指针
        buf->retrieve(2 + topic_len + 2 + msg_name_len + 4 + msg_len);
    }
}

void PollManager::handleMessage(const std::string& topic,
                                const std::string& msg_name,
                                const std::string& data) {

    // LOG_INFO << "Received message on topic [" << topic << "], type: " << msg_name;
    if (msg_name == "TopicTargetsUpdate") {
        TopicTargetsUpdate update;
        if (!update.ParseFromString(data)) {
            LOG_WARN << "Failed to parse TopicTargetsUpdate for topic: " << topic;
            return;
        }

        auto& targets = topic_targets_[update.topic()];

        // 增加目标
        for (const auto& n : update.add_targets()) {
            targets.insert(n);
        }

        // 移除目标
        for (const auto& n : update.remove_targets()) {
            targets.erase(n);
        }

        LOG_INFO << "Updated targets for topic: " << update.topic()
                 << " (+" << update.add_targets_size()
                 << ", -" << update.remove_targets_size() << ")";
        return;
    }
    
    // 创建消息并解析
    auto msg = MsgFactory::instance().createMessage(msg_name);
    if (!msg || !msg->ParseFromString(data)) {
        LOG_WARN << "Failed to create/parse message: " << msg_name;
        return;
    }
    
    // 推送到消息队列
    if (auto mq = SystemManager::instance().getMessageQueue()) {
        mq->push(topic, MsgFactory::instance().makeSharedMessage(std::move(msg)));
    }
}


std::unordered_set<NodeInfo, NodeInfoHash, NodeInfoEqual> PollManager::getTargets(const std::string& topic) const {
    auto it = topic_targets_.find(topic);
    if (it == topic_targets_.end()) return {};
    return it->second;
}

