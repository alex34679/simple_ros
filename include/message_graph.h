#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <grpcpp/grpcpp.h>
#include "ros_rpc.grpc.pb.h"
namespace simple_ros
{

    // ========== 图数据结构 ==========
    struct TopicKey
    {
        std::string topic;
        std::string msg_type;
        bool operator==(const TopicKey &o) const noexcept
        {
            return topic == o.topic && msg_type == o.msg_type;
        }
    };
    struct TopicKeyHash
    {
        size_t operator()(const TopicKey &k) const noexcept
        {
            return std::hash<std::string>()(k.topic) ^ (std::hash<std::string>()(k.msg_type) << 1);
        }
    };

    struct Edge
    {
        std::string src_node; // 发布者
        std::string dst_node; // 订阅者
        TopicKey key;         // 话题+类型
        bool operator==(const Edge &o) const noexcept
        {
            return src_node == o.src_node && dst_node == o.dst_node &&
                   key.topic == o.key.topic && key.msg_type == o.key.msg_type;
        }
    };
    struct EdgeHash
    {
        size_t operator()(const Edge &e) const noexcept
        {
            size_t h1 = std::hash<std::string>()(e.src_node);
            size_t h2 = std::hash<std::string>()(e.dst_node);
            size_t h3 = std::hash<std::string>()(e.key.topic);
            size_t h4 = std::hash<std::string>()(e.key.msg_type);
            return (((h1 ^ (h2 << 1)) ^ (h3 << 1)) ^ (h4 << 1));
        }
    };

    struct NodeVertex
    {
        NodeInfo info; // 来自 .proto
        // 该节点发布/订阅的 (topic,msg)
        std::unordered_set<TopicKey, TopicKeyHash> publishes;
        std::unordered_set<TopicKey, TopicKeyHash> subscribes;
    };

    class MessageGraph
    {
    public:
        // 新增或更新节点信息
        void UpsertNode(const NodeInfo &info);

        // 维护 topic/msg 到 发布者/订阅者 的索引，并即时建边
        void AddPublisher(const NodeInfo &node, const TopicKey &k);
        void AddSubscriber(const NodeInfo &node, const TopicKey &k);

        // 删除发布/订阅关系，并相应删边；必要时清理孤立点
        void RemovePublisher(const NodeInfo &node, const TopicKey &k);
        void RemoveSubscriber(const NodeInfo &node, const TopicKey &k);

        std::vector<NodeInfo> GetSubscribersByTopic(const std::string &topic) const;
        std::vector<NodeInfo> GetPublishersByTopic(const std::string &topic) const;

        // 通过节点名获取节点信息
        bool GetNodeByName(const std::string &node_name, NodeInfo *node_info) const
        {
            auto it = nodes_.find(node_name);
            if (it != nodes_.end())
            {
                *node_info = it->second.info;
                return true;
            }
            return false;
        }

        // 导出/打印
        std::string ToReadableString() const;
        std::string ToDOT() const;  // graphviz
        std::string ToJSON() const; // 便于外部脚本二次处理

        // 获取所有节点
        std::vector<NodeInfo> GetAllNodes() const
        {
            std::vector<NodeInfo> result;
            for (const auto &pair : nodes_)
            {
                result.push_back(pair.second.info);
            }
            return result;
        }

        // 检查节点是否存在
        bool HasNode(const std::string &node_name) const
        {
            return nodes_.find(node_name) != nodes_.end();
        }

        // 获取节点发布的所有话题
        std::vector<std::string> GetNodePublishTopics(const std::string &node_name) const
        {
            std::vector<std::string> result;
            auto it = nodes_.find(node_name);
            if (it != nodes_.end())
            {
                for (const auto &topic_key : it->second.publishes)
                {
                    result.push_back(topic_key.topic);
                }
            }
            return result;
        }

        // 获取节点订阅的所有话题
        std::vector<std::string> GetNodeSubscribeTopics(const std::string &node_name) const
        {
            std::vector<std::string> result;
            auto it = nodes_.find(node_name);
            if (it != nodes_.end())
            {
                for (const auto &topic_key : it->second.subscribes)
                {
                    result.push_back(topic_key.topic);
                }
            }
            return result;
        }

        // 获取节点发布的所有话题（包含消息类型）
        std::vector<TopicKey> GetNodePublishTopicKeys(const std::string &node_name) const
        {
            std::vector<TopicKey> result;
            auto it = nodes_.find(node_name);
            if (it != nodes_.end())
            {
                for (const auto &topic_key : it->second.publishes)
                {
                    result.push_back(topic_key);
                }
            }
            return result;
        }

        // 获取节点订阅的所有话题（包含消息类型）
        std::vector<TopicKey> GetNodeSubscribeTopicKeys(const std::string &node_name) const
        {
            std::vector<TopicKey> result;
            auto it = nodes_.find(node_name);
            if (it != nodes_.end())
            {
                for (const auto &topic_key : it->second.subscribes)
                {
                    result.push_back(topic_key);
                }
            }
            return result;
        }

    private:
        // 节点名 → 顶点
        std::unordered_map<std::string, NodeVertex> nodes_;

        // 话题索引：快速匹配发布者与订阅者
        std::unordered_map<TopicKey, std::unordered_set<std::string>, TopicKeyHash> publishers_by_topic_;
        std::unordered_map<TopicKey, std::unordered_set<std::string>, TopicKeyHash> subscribers_by_topic_;

        // 边集合（去重）
        std::unordered_set<Edge, EdgeHash> edges_;

        // 辅助：根据当前索引把匹配的节点之间加/删边
        void ConnectPublisherToSubscribers(const std::string &pub_node, const TopicKey &k);
        void ConnectPublishersToSubscriber(const std::string &sub_node, const TopicKey &k);
        void RemoveEdgesBy(const std::string &node, const TopicKey &k, bool node_is_publisher);

        // 可选：清理完全没有发布/订阅与边的孤立节点（避免积累）
        void CleanupIsolatedNodeIfAny(const std::string &node_name);
    };

}