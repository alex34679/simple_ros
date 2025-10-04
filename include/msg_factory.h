#pragma once

#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <memory>
#include <string>
#include <unordered_map>

class MsgFactory {
public:
    // 获取单例
    static MsgFactory& instance();

    // ------------------------------
    // 注册消息类型（模板函数必须在头文件）
    // ------------------------------
    template<typename MsgType>
    void registerMessage() {
        factory_[MsgType::descriptor()->full_name()] = &MsgType::default_instance();
    }

    // ------------------------------
    // 创建 unique_ptr<Message>
    // 如果未注册，尝试通过 DescriptorPool 自动检测
    // ------------------------------
    std::unique_ptr<google::protobuf::Message> createMessage(const std::string& name);

    // ------------------------------
    // 将 unique_ptr 转换为 shared_ptr
    // ------------------------------
    std::shared_ptr<google::protobuf::Message> makeSharedMessage(std::unique_ptr<google::protobuf::Message> msg);

private:
    MsgFactory() = default;
    ~MsgFactory() = default;

    // 禁止拷贝
    MsgFactory(const MsgFactory&) = delete;
    MsgFactory& operator=(const MsgFactory&) = delete;

    // 缓存消息原型
    std::unordered_map<std::string, const google::protobuf::Message*> factory_;
    google::protobuf::DynamicMessageFactory dynamic_factory_;
};
