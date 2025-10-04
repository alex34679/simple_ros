#pragma once

#include <string>
#include <memory>
#include <map>
#include <thread>
#include <chrono>

#include <google/protobuf/message.h>
#include <foxglove/server.hpp>
#include <foxglove/channel.hpp>
#include <foxglove/schemas.hpp>

#include "ros_rpc_client.h"
#include "node_handle.h"
#include "example.pb.h"
#include "marker.pb.h"

#include <muduo/base/Logging.h>
#include <nlohmann/json.hpp>

class FoxgloveBridge {
public:
    FoxgloveBridge(const std::string& rpc_server_address,
                   const std::string& host = "127.0.0.1",
                   uint16_t port = 8765);
    ~FoxgloveBridge();

    bool init();   // 初始化 Foxglove server 和 RPC client
    bool start();  // 启动自动发现与订阅线程
    void stop();   // 停止线程，关闭服务器

    // 消息回调：接收到任意 protobuf 消息时调用
    void onGenericMessage(const std::string& topic_name,
                          const std::shared_ptr<google::protobuf::Message>& msg);

private:
    void pollAndSubscribeLoop(); // 自动发现 & 订阅 RPC 话题循环

    // JSON channel 相关
    std::shared_ptr<foxglove::RawChannel> createOrGetJsonChannel(const std::string& topic,
                                                                    const std::string& msg_type);
    std::string pbFieldTypeToJsonType(const google::protobuf::FieldDescriptor* field);
    std::string buildStableJsonSchema(const google::protobuf::Descriptor* desc);
    void publishJsonMessage(const std::string& topic,
                        const std::shared_ptr<google::protobuf::Message>& msg);

    // Marker 专用处理
    void publishCube(const std::shared_ptr<foxglove::schemas::SceneUpdateChannel>& channel,
                     const std::shared_ptr<visualization_msgs::Marker>& marker);
    void updateTrajectory(const std::shared_ptr<foxglove::schemas::SceneUpdateChannel>& channel,
                          const std::shared_ptr<visualization_msgs::Marker>& marker);
    void publishCylinder(const std::shared_ptr<foxglove::schemas::SceneUpdateChannel>& scene_channel,
                         const std::shared_ptr<visualization_msgs::Marker>& marker);
    void onMarkerArrayMessage(const std::string& topic_name,
                              const std::shared_ptr<visualization_msgs::MarkerArray>& marker_array);

    // 为每个 topic 创建/获取独立 SceneUpdateChannel
    std::shared_ptr<foxglove::schemas::SceneUpdateChannel> createOrGetSceneChannel(
        const std::string& topic_name);

private:
    // RPC 地址
    std::string rpc_server_address_;

    // Foxglove WebSocket Server 配置
    std::string host_;
    uint16_t port_;

    // Foxglove server
    std::unique_ptr<foxglove::WebSocketServer> server_;

    // RPC client
    std::unique_ptr<simple_ros::RosRpcClient> rpc_client_;

    // 管理不同消息类型的 JSON channel
    std::map<std::string, std::shared_ptr<foxglove::RawChannel>> json_channels_;

    // 管理不同 topic 的 SceneUpdateChannel
    std::map<std::string, std::shared_ptr<foxglove::schemas::SceneUpdateChannel>> scene_channels_;

    // 保存订阅者，防止析构
    std::map<std::string, std::shared_ptr<Subscriber>> subscribers_;

    // 控制 poll 线程运行状态
    bool running_;
    std::thread poll_thread_;

    // 系统对象，用于 spinOnce
    SystemManager& sys_ = SystemManager::instance();
};
