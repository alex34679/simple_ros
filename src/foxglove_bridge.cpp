#include "foxglove_bridge.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/util/json_util.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <muduo/base/Logging.h>
#include "subscription_handler_registry.h"



using json = nlohmann::json;

FoxgloveBridge::FoxgloveBridge(const std::string& rpc_server_address,
                               const std::string& host,
                               uint16_t port)
    : rpc_server_address_(rpc_server_address), host_(host), port_(port),
      running_(false) {}

FoxgloveBridge::~FoxgloveBridge() {
    stop();
}

bool FoxgloveBridge::init() {
    foxglove::WebSocketServerOptions opts;
    opts.host = host_;
    opts.port = port_;
    auto server_result = foxglove::WebSocketServer::create(std::move(opts));
    if (!server_result.has_value()) {
        LOG_ERROR << "Failed to create foxglove server: "
                  << foxglove::strerror(server_result.error());
        return false;
    }
    server_ = std::make_unique<foxglove::WebSocketServer>(std::move(server_result.value()));
    LOG_INFO << "Foxglove server initialized at " << host_ << ":" << port_;

    rpc_client_ = std::make_unique<simple_ros::RosRpcClient>(rpc_server_address_);
    LOG_INFO << "RPC client initialized for server: " << rpc_server_address_;

    sys_.init(50053, "foxglove_bridge_node");

    // 不要创建固定的 scene_channel_，延迟到订阅时再创建
    return true;
}


std::shared_ptr<foxglove::schemas::SceneUpdateChannel>
FoxgloveBridge::createOrGetSceneChannel(const std::string& topic_name) {
    // scene 专用 key，避免和 json channel 撞名
    std::string scene_topic = topic_name + "/scene";

    auto it = scene_channels_.find(scene_topic);
    if (it != scene_channels_.end()) {
        return it->second;
    }

    auto channel_res = foxglove::schemas::SceneUpdateChannel::create(scene_topic);
    if (!channel_res.has_value()) {
        LOG_ERROR << "Failed to create SceneUpdateChannel for " << scene_topic
                  << ": " << foxglove::strerror(channel_res.error());
        return nullptr;
    }

    auto channel = std::make_shared<foxglove::schemas::SceneUpdateChannel>(
        std::move(channel_res.value()));
    scene_channels_[scene_topic] = channel;
    LOG_INFO << "Created SceneUpdateChannel for topic: " << scene_topic;
    return channel;
}


bool FoxgloveBridge::start() {
    if (!server_) {
        LOG_ERROR << "Server not initialized. Call init() first.";
        return false;
    }

    running_ = true;
    poll_thread_ = std::thread(&FoxgloveBridge::pollAndSubscribeLoop, this);
    LOG_INFO << "FoxgloveBridge started.";
    return true;
}

void FoxgloveBridge::stop() {
    running_ = false;
    if (poll_thread_.joinable())
        poll_thread_.join();
    server_.reset();
    LOG_INFO << "FoxgloveBridge stopped.";
}

void FoxgloveBridge::pollAndSubscribeLoop() {
    NodeHandle nh;
    auto& registry = SubscriptionHandlerRegistry::getInstance();  // 单例
    auto last_poll = std::chrono::steady_clock::now();

    while (running_) {
        // 1️⃣ 高频 spin（处理回调）
        sys_.spinOnce();

        // 2️⃣ 每秒查询一次 RPC topic
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_poll).count() >= 1) {
            last_poll = now;

            try {
                simple_ros::GetTopicsResponse resp;
                if (!rpc_client_->GetTopics("", &resp)) {
                    LOG_WARN << "Failed to GetTopics from RPC server";
                } else {
                    for (const auto& t : resp.topics()) {
                        const std::string topic_name = t.topic_name();
                        const std::string msg_type = t.msg_type();

                        // 已经订阅过，跳过
                        if (subscribers_.find(topic_name) != subscribers_.end()) continue;

                        // 先确保 JSON channel 存在
                        try {
                            auto channel = createOrGetJsonChannel(topic_name, msg_type);
                        } catch (const std::exception& e) {
                            LOG_ERROR << "Failed to create json channel for "
                                      << msg_type << ": " << e.what();
                            continue;
                        }

                        // 使用 SubscriptionHandlerRegistry 创建订阅
                        try {
                            auto cb = [this, topic_name](const std::shared_ptr<google::protobuf::Message>& msg) {
                                this->onGenericMessage(topic_name, msg);
                            };

                            auto sub = registry.createSubscription(nh, topic_name, msg_type, cb);
                            if (!sub) {
                                LOG_ERROR << "Failed to subscribe to topic: " << topic_name;
                                continue;
                            }

                            subscribers_[topic_name] = sub;  // 保存 Subscriber 防止析构
                        } catch (const std::exception& e) {
                            LOG_ERROR << "Failed to subscribe topic " << topic_name
                                      << ": " << e.what();
                        } catch (...) {
                            LOG_ERROR << "Unknown error subscribing topic " << topic_name;
                        }
                    }
                }
            } catch (const std::exception& e) {
                LOG_ERROR << "Exception in poll loop: " << e.what();
            } catch (...) {
                LOG_ERROR << "Unknown exception in poll loop";
            }
        }


    }
}



void FoxgloveBridge::onGenericMessage(
    const std::string& topic_name,
    const std::shared_ptr<google::protobuf::Message>& msg) 
{
    if (!msg) return;

    // 1️⃣ 先发布 JSON
    publishJsonMessage(topic_name, msg);

    // 2️⃣ Marker 或 MarkerArray 判断
    const std::string msg_type = msg->GetDescriptor()->full_name();

    if (msg_type == "visualization_msgs.Marker") {
        auto marker = std::dynamic_pointer_cast<visualization_msgs::Marker>(msg);
        if (!marker) return;

        // 原来的单个 marker 发布逻辑
        auto scene_channel = createOrGetSceneChannel(topic_name);
        if (!scene_channel) return;

        switch (marker->type()) {
            case visualization_msgs::MarkerType::CUBE:
                publishCube(scene_channel, marker);
                break;
            case visualization_msgs::MarkerType::CYLINDER:
                publishCylinder(scene_channel, marker);
                break;
            case visualization_msgs::MarkerType::LINE_STRIP:
                updateTrajectory(scene_channel, marker);
                break;
            default:
                break;
        }

    } else if (msg_type == "visualization_msgs.MarkerArray") {
        auto marker_array = std::dynamic_pointer_cast<visualization_msgs::MarkerArray>(msg);
        if (!marker_array) return;
        onMarkerArrayMessage(topic_name, marker_array);
    }
}


// ------------------- 辅助函数 -------------------

void FoxgloveBridge::publishJsonMessage(
    const std::string& topic,
    const std::shared_ptr<google::protobuf::Message>& msg) 
{
    std::string json_str;
    google::protobuf::util::JsonPrintOptions opts;
    opts.add_whitespace = false;
    opts.always_print_primitive_fields = true;
    opts.preserve_proto_field_names = true;

    auto status = google::protobuf::util::MessageToJsonString(*msg, &json_str, opts);
    if (!status.ok()) return;

    std::string msg_type = msg->GetDescriptor()->full_name();
    auto channel_it = json_channels_.find(topic);
    std::shared_ptr<foxglove::RawChannel> channel;
    if (channel_it == json_channels_.end()) {
        try {
            channel = createOrGetJsonChannel(topic, msg_type);
        } catch (...) { return; }
    } else {
        channel = channel_it->second;
    }

    if (channel) {
        channel->log(reinterpret_cast<const std::byte*>(json_str.data()), json_str.size());
    }
}

std::shared_ptr<foxglove::RawChannel>
FoxgloveBridge::createOrGetJsonChannel(const std::string& topic,
                                       const std::string& msg_type) 
{
    // 先查缓存
    auto it = json_channels_.find(topic);
    if (it != json_channels_.end()) return it->second;

    // 生成 schema
    std::string schema_text;
    const auto* desc = google::protobuf::DescriptorPool::generated_pool()
                           ->FindMessageTypeByName(msg_type);

    if (desc) {
        schema_text = buildStableJsonSchema(desc);
    } else {
        nlohmann::json j;
        j["type"] = "object";
        j["additionalProperties"] = true;
        schema_text = j.dump();
        LOG_WARN << "Using default schema for unknown message type: " << msg_type;
    }

    // 创建 RawChannel
    foxglove::Schema schema;
    schema.encoding = "jsonschema";
    schema.data_len = schema_text.size();

    auto storage = std::make_shared<std::vector<std::byte>>(schema_text.size());
    memcpy(storage->data(), schema_text.data(), schema_text.size());
    schema.data = storage->data();

    // 这里用 topic 作为频道名，而不是 msg_type
    auto channel_res = foxglove::RawChannel::create(topic, "json", schema);
    if (!channel_res.has_value()) {
        throw std::runtime_error(foxglove::strerror(channel_res.error()));
    }

    auto channel = std::make_shared<foxglove::RawChannel>(std::move(channel_res.value()));
    json_channels_[topic] = channel;  // 按 topic 缓存
    return channel;
}


void FoxgloveBridge::publishCylinder(
    const std::shared_ptr<foxglove::schemas::SceneUpdateChannel>& scene_channel,
    const std::shared_ptr<visualization_msgs::Marker>& marker) 
{
    if (!scene_channel || !marker) return;

    foxglove::schemas::CylinderPrimitive cylinder;
    cylinder.size = foxglove::schemas::Vector3{
        marker->scale().x(),
        marker->scale().y(),
        marker->scale().z()
    };
    cylinder.color = foxglove::schemas::Color{
        marker->color().r(),
        marker->color().g(),
        marker->color().b(),
        marker->color().a()
    };

    foxglove::schemas::Pose pose;
    pose.position = foxglove::schemas::Vector3{
        marker->pose().position().x(),
        marker->pose().position().y(),
        marker->pose().position().z()
    };
    pose.orientation = foxglove::schemas::Quaternion{
        marker->pose().orientation().x(),
        marker->pose().orientation().y(),
        marker->pose().orientation().z(),
        marker->pose().orientation().w()
    };
    cylinder.pose = pose;

    cylinder.bottom_scale = 1.0;
    cylinder.top_scale = 1.0;

    foxglove::schemas::SceneEntity entity;
    entity.id = marker->ns() + "_cylinder_" + std::to_string(marker->id());
    entity.cylinders.push_back(cylinder);

    foxglove::schemas::SceneUpdate update;
    update.entities.push_back(entity);
    scene_channel->log(update);
}


void FoxgloveBridge::publishCube(
    const std::shared_ptr<foxglove::schemas::SceneUpdateChannel>& scene_channel,
    const std::shared_ptr<visualization_msgs::Marker>& marker) 
{
    if (!scene_channel) return;

    foxglove::schemas::CubePrimitive cube;
    cube.size = foxglove::schemas::Vector3{
        marker->scale().x(),
        marker->scale().y(),
        marker->scale().z()
    };
    cube.color = foxglove::schemas::Color{
        marker->color().r(),
        marker->color().g(),
        marker->color().b(),
        marker->color().a()
    };
    foxglove::schemas::Pose pose;
    pose.position = foxglove::schemas::Vector3{
        marker->pose().position().x(),
        marker->pose().position().y(),
        marker->pose().position().z()
    };
    pose.orientation = foxglove::schemas::Quaternion{
        marker->pose().orientation().x(),
        marker->pose().orientation().y(),
        marker->pose().orientation().z(),
        marker->pose().orientation().w()
    };
    cube.pose = pose;

    foxglove::schemas::SceneEntity entity;
    entity.id = marker->ns() + "_cube"; // 保证每个topic独立
    entity.cubes.push_back(cube);

    foxglove::schemas::SceneUpdate update;
    update.entities.push_back(entity);
    scene_channel->log(update);
}

void FoxgloveBridge::updateTrajectory(
    const std::shared_ptr<foxglove::schemas::SceneUpdateChannel>& scene_channel,
    const std::shared_ptr<visualization_msgs::Marker>& marker) 
{
    if (!scene_channel || !marker) return;

    static std::unordered_map<std::string, std::deque<foxglove::schemas::Point3>> traj_points_map;
    auto& traj_points = traj_points_map[marker->ns()];

    if (marker->points_size() > 0) {
        // 如果 marker 自带点集，直接取出来
        for (const auto& pt : marker->points()) {
            traj_points.push_back({
                pt.x(),
                pt.y(),
                pt.z()
            });
        }
    } else {
        // 否则退化为使用 pose().position()
        traj_points.push_back({
            marker->pose().position().x(),
            marker->pose().position().y(),
            marker->pose().position().z()
        });
    }

    if(marker->lifetime() < 0){
        // 永久保留
        while (traj_points.size() > 10000) { // 限制最大点数，防止内存无限增长
            traj_points.pop_front();
        }
    } else if(marker->lifetime() == 0){
        // 只保留最新的一个点
        while (traj_points.size() > 1) {
            traj_points.pop_front();
        }
    } else if(marker->lifetime() > 0){
        // 保留最近 N 个点
        while (traj_points.size() > (int)marker->lifetime()) { 
            traj_points.pop_front();
        }
    }


    foxglove::schemas::SceneEntity traj_entity;
    traj_entity.id = marker->ns() + "_traj";

    if (!traj_points.empty()) {
        foxglove::schemas::LinePrimitive line;
        line.type = foxglove::schemas::LinePrimitive::LineType::LINE_STRIP;
        line.thickness = std::max({marker->scale().x(), marker->scale().y(), marker->scale().z()}) * 0.2;
        line.scale_invariant = false;
        line.color = foxglove::schemas::Color{
            marker->color().r(),
            marker->color().g(),
            marker->color().b(),
            marker->color().a()
        };
        line.points.assign(traj_points.begin(), traj_points.end());
        traj_entity.lines.push_back(line);
    }

    foxglove::schemas::SceneUpdate update;
    update.entities.push_back(traj_entity);
    scene_channel->log(update);
}


void FoxgloveBridge::onMarkerArrayMessage(
    const std::string& topic_name,
    const std::shared_ptr<visualization_msgs::MarkerArray>& marker_array) 
{
    if (!marker_array) return;

    auto scene_channel = createOrGetSceneChannel(topic_name);
    if (!scene_channel) return;

    foxglove::schemas::SceneUpdate update;

    for (const auto& marker : marker_array->markers()) {
        foxglove::schemas::SceneEntity entity;
        entity.id = marker.ns() + "_" + std::to_string(marker.id());

        switch (marker.type()) {
            case visualization_msgs::MarkerType::CUBE: {
                foxglove::schemas::CubePrimitive cube;
                cube.size = {marker.scale().x(), marker.scale().y(), marker.scale().z()};
                cube.color = {marker.color().r(), marker.color().g(), marker.color().b(), marker.color().a()};
                foxglove::schemas::Pose pose;
                pose.position = {marker.pose().position().x(), marker.pose().position().y(), marker.pose().position().z()};
                pose.orientation = {marker.pose().orientation().x(), marker.pose().orientation().y(),
                                         marker.pose().orientation().z(), marker.pose().orientation().w()};
                cube.pose = pose;
                entity.cubes.push_back(cube);
                break;
            }
            case visualization_msgs::MarkerType::CYLINDER: {
                foxglove::schemas::CylinderPrimitive cylinder;
                cylinder.size = {marker.scale().x(), marker.scale().y(), marker.scale().z()};
                cylinder.color = {marker.color().r(), marker.color().g(), marker.color().b(), marker.color().a()};
                foxglove::schemas::Pose pose;
                pose.position = {marker.pose().position().x(), marker.pose().position().y(), marker.pose().position().z()};
                pose.orientation = {marker.pose().orientation().x(), marker.pose().orientation().y(),
                                             marker.pose().orientation().z(), marker.pose().orientation().w()};
                cylinder.pose = pose;
                cylinder.bottom_scale = 1.0;
                cylinder.top_scale = 1.0;
                entity.cylinders.push_back(cylinder);
                break;
            }
            case visualization_msgs::MarkerType::LINE_STRIP: {
                foxglove::schemas::LinePrimitive line;
                line.type = foxglove::schemas::LinePrimitive::LineType::LINE_STRIP;
                line.points.reserve(marker.points_size());
                for (const auto& pt : marker.points()) {
                    line.points.push_back({pt.x(), pt.y(), pt.z()});
                }
                line.color = {marker.color().r(), marker.color().g(), marker.color().b(), marker.color().a()};
                line.thickness = std::max({marker.scale().x(), marker.scale().y(), marker.scale().z()}) * 0.2;
                entity.lines.push_back(line);
                break;
            }
            default:
                break; // 可根据需要扩展其他类型
        }

        update.entities.push_back(entity);
    }

    scene_channel->log(update);
}



std::string FoxgloveBridge::pbFieldTypeToJsonType(const google::protobuf::FieldDescriptor* field) {
    using fd = google::protobuf::FieldDescriptor;
    switch (field->cpp_type()) {
        case fd::CPPTYPE_INT32:
        case fd::CPPTYPE_INT64:
        case fd::CPPTYPE_UINT32:
        case fd::CPPTYPE_UINT64:
        case fd::CPPTYPE_FLOAT:
        case fd::CPPTYPE_DOUBLE:
            return "number";
        case fd::CPPTYPE_BOOL:
            return "boolean";
        case fd::CPPTYPE_STRING:
            return "string";
        case fd::CPPTYPE_ENUM:
            return "string";
        case fd::CPPTYPE_MESSAGE:
            return "object";
        default:
            return "string";
    }
}


std::string FoxgloveBridge::buildStableJsonSchema(const google::protobuf::Descriptor* desc) {
    json schema;
    schema["title"] = desc->full_name();
    schema["type"] = "object";
    json props = json::object();

    // 先收集字段名，再排序
    std::vector<const google::protobuf::FieldDescriptor*> fields;
    for (int i = 0; i < desc->field_count(); ++i) {
        fields.push_back(desc->field(i));
    }
    std::sort(fields.begin(), fields.end(),
              [](const auto* a, const auto* b){ return a->name() < b->name(); });

    for (const auto* field : fields) {
        json f;
        if (field->is_repeated()) {
            f["type"] = "array";
            json items;
            if (field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
                const auto* subdesc = field->message_type();
                items["type"] = "object";
                items["properties"] = json::object();
                for (int j = 0; j < subdesc->field_count(); ++j) {
                    const auto* sf = subdesc->field(j);
                    items["properties"][sf->name()]["type"] = pbFieldTypeToJsonType(sf);
                }
            } else {
                items["type"] = pbFieldTypeToJsonType(field);
            }
            f["items"] = items;
        } else if (field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
            const auto* subdesc = field->message_type();
            json subprops = json::object();
            for (int j = 0; j < subdesc->field_count(); ++j) {
                const auto* sf = subdesc->field(j);
                subprops[sf->name()]["type"] = pbFieldTypeToJsonType(sf);
            }
            f["type"] = "object";
            f["properties"] = subprops;
        } else {
            f["type"] = pbFieldTypeToJsonType(field);
        }
        props[field->name()] = f;
    }

    schema["properties"] = props;
    return schema.dump();
}
