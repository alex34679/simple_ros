
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <nlohmann/json.hpp> // 若不想引入第三方，可手写 JSON 字符串
#include "message_graph.h"
#include <grpcpp/server_builder.h>
namespace simple_ros {
// ========== MessageGraph 实现 ==========
void MessageGraph::UpsertNode(const NodeInfo& info) {
    auto& v = nodes_[info.node_name()];
    v.info = info; // 覆盖更新其 meta（ip/port/…）
}

void MessageGraph::ConnectPublisherToSubscribers(const std::string& pub_node, const TopicKey& k) {
    auto it = subscribers_by_topic_.find(k);
    if (it == subscribers_by_topic_.end()) return;
    for (const auto& sub : it->second) {
        edges_.insert(Edge{pub_node, sub, k});
    }
}

void MessageGraph::ConnectPublishersToSubscriber(const std::string& sub_node, const TopicKey& k) {
    auto it = publishers_by_topic_.find(k);
    if (it == publishers_by_topic_.end()) return;
    for (const auto& pub : it->second) {
        edges_.insert(Edge{pub, sub_node, k});
    }
}

void MessageGraph::AddPublisher(const NodeInfo& node, const TopicKey& k) {
    UpsertNode(node);
    nodes_[node.node_name()].publishes.insert(k);
    publishers_by_topic_[k].insert(node.node_name());
    ConnectPublisherToSubscribers(node.node_name(), k);
}

void MessageGraph::AddSubscriber(const NodeInfo& node, const TopicKey& k) {
    UpsertNode(node);
    nodes_[node.node_name()].subscribes.insert(k);
    subscribers_by_topic_[k].insert(node.node_name());
    ConnectPublishersToSubscriber(node.node_name(), k);
}

void MessageGraph::RemoveEdgesBy(const std::string& node, const TopicKey& k, bool node_is_publisher) {
    // 由于 edges_ 是 unordered_set，只能线性扫描；但边规模通常 << 节点规模，且仅在注销/退订时发生。
    std::vector<Edge> to_erase;
    to_erase.reserve(16);
    for (const auto& e : edges_) {
        if (e.key.topic == k.topic && e.key.msg_type == k.msg_type) {
            if (node_is_publisher && e.src_node == node) to_erase.push_back(e);
            if (!node_is_publisher && e.dst_node == node) to_erase.push_back(e);
        }
    }
    for (auto& e : to_erase) edges_.erase(e);
}

void MessageGraph::RemovePublisher(const NodeInfo& node, const TopicKey& k) {
    auto itn = nodes_.find(node.node_name());
    if (itn != nodes_.end()) {
        itn->second.publishes.erase(k);
    }
    auto itp = publishers_by_topic_.find(k);
    if (itp != publishers_by_topic_.end()) {
        itp->second.erase(node.node_name());
        if (itp->second.empty()) publishers_by_topic_.erase(itp);
    }
    RemoveEdgesBy(node.node_name(), k, /*node_is_publisher=*/true);
    CleanupIsolatedNodeIfAny(node.node_name());
}

void MessageGraph::RemoveSubscriber(const NodeInfo& node, const TopicKey& k) {
    auto itn = nodes_.find(node.node_name());
    if (itn != nodes_.end()) {
        itn->second.subscribes.erase(k);
    }
    auto its = subscribers_by_topic_.find(k);
    if (its != subscribers_by_topic_.end()) {
        its->second.erase(node.node_name());
        if (its->second.empty()) subscribers_by_topic_.erase(its);
    }
    RemoveEdgesBy(node.node_name(), k, /*node_is_publisher=*/false);
    CleanupIsolatedNodeIfAny(node.node_name());
}

void MessageGraph::CleanupIsolatedNodeIfAny(const std::string& node_name) {
    auto it = nodes_.find(node_name);
    if (it == nodes_.end()) return;

    bool has_pub = !it->second.publishes.empty();
    bool has_sub = !it->second.subscribes.empty();
    if (has_pub || has_sub) return;

    // 确保没有边指向/来自该点
    for (const auto& e : edges_) {
        if (e.src_node == node_name || e.dst_node == node_name) return;
    }
    nodes_.erase(it);
}

std::string MessageGraph::ToReadableString() const {
    std::ostringstream oss;
    oss << "==== Message Graph ====\n";
    oss << "Nodes: " << nodes_.size() << ", Edges: " << edges_.size() << "\n\n";
    oss << "[Nodes]\n";
    for (const auto& [name, v] : nodes_) {
        oss << " - " << name << " (ip=" << v.info.ip() << ", port=" << v.info.port() << ")\n";
        if (!v.publishes.empty()) {
            oss << "    publishes:\n";
            for (const auto& k : v.publishes) oss << "      - " << k.topic << " : " << k.msg_type << "\n";
        }
        if (!v.subscribes.empty()) {
            oss << "    subscribes:\n";
            for (const auto& k : v.subscribes) oss << "      - " << k.topic << " : " << k.msg_type << "\n";
        }
    }
    oss << "\n[Edges]\n";
    for (const auto& e : edges_) {
        oss << " - " << e.src_node << " -> " << e.dst_node
            << "  [" << e.key.topic << " : " << e.key.msg_type << "]\n";
    }
    return oss.str();
}

std::string MessageGraph::ToDOT() const {
    std::ostringstream oss;
    oss << "digraph RosGraph {\n";
    oss << "  rankdir=LR;\n  node [shape=box, style=rounded];\n";
    for (const auto& [name, v] : nodes_) {
        oss << "  \"" << name << "\";\n";
    }
    for (const auto& e : edges_) {
        oss << "  \"" << e.src_node << "\" -> \"" << e.dst_node
            << "\" [label=\"" << e.key.topic << "\\n" << e.key.msg_type << "\"];\n";
    }
    oss << "}\n";
    return oss.str();
}

std::string MessageGraph::ToJSON() const {
    nlohmann::json j;
    j["nodes"] = nlohmann::json::array();
    for (const auto& [name, v] : nodes_) {
        nlohmann::json n;
        n["name"] = name;
        n["ip"] = v.info.ip();
        n["port"] = v.info.port();
        n["publishes"] = nlohmann::json::array();
        for (const auto& k : v.publishes) n["publishes"].push_back({{"topic",k.topic},{"msg",k.msg_type}});
        n["subscribes"] = nlohmann::json::array();
        for (const auto& k : v.subscribes) n["subscribes"].push_back({{"topic",k.topic},{"msg",k.msg_type}});
        j["nodes"].push_back(std::move(n));
    }
    j["edges"] = nlohmann::json::array();
    for (const auto& e : edges_) {
        j["edges"].push_back({
            {"src", e.src_node},
            {"dst", e.dst_node},
            {"topic", e.key.topic},
            {"msg", e.key.msg_type}
        });
    }
    return j.dump(2);
}


std::vector<NodeInfo> MessageGraph::GetSubscribersByTopic(const std::string& topic) const {
    std::vector<NodeInfo> result;

    // 找到对应 topic 的订阅者集合
    auto it = std::find_if(subscribers_by_topic_.begin(), subscribers_by_topic_.end(),
                           [&](const auto& pair){ return pair.first.topic == topic; });
    if (it == subscribers_by_topic_.end()) return result;

    // 一次循环获取 NodeInfo
    result.reserve(it->second.size());
    for (const auto& node_name : it->second) {
        if (auto nit = nodes_.find(node_name); nit != nodes_.end())
            result.push_back(nit->second.info);
    }
    return result;
}

std::vector<NodeInfo> MessageGraph::GetPublishersByTopic(const std::string& topic) const {
    std::vector<NodeInfo> result;

    auto it = std::find_if(publishers_by_topic_.begin(), publishers_by_topic_.end(),
                           [&](const auto& pair){ return pair.first.topic == topic; });
    if (it == publishers_by_topic_.end()) return result;

    result.reserve(it->second.size());
    for (const auto& node_name : it->second) {
        if (auto nit = nodes_.find(node_name); nit != nodes_.end())
            result.push_back(nit->second.info);
    }
    return result;
}


}