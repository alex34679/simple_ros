#include "msg_factory.h"

// 获取单例
MsgFactory& MsgFactory::instance() {
    static MsgFactory inst;
    return inst;
}

// 创建 unique_ptr<Message>
std::unique_ptr<google::protobuf::Message> MsgFactory::createMessage(const std::string& name) {
    // 1. 尝试从缓存获取
    auto it = factory_.find(name);
    if (it != factory_.end()) {
        return std::unique_ptr<google::protobuf::Message>(it->second->New());
    }

    // 2. 未注册类型，通过 DescriptorPool 查找
    const google::protobuf::Descriptor* desc =
        google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(name);
    if (!desc) return nullptr;

    const google::protobuf::Message* prototype = dynamic_factory_.GetPrototype(desc);
    if (!prototype) return nullptr;

    // 3. 缓存起来，下次直接使用
    factory_[name] = prototype;

    // 4. 返回 unique_ptr
    return std::unique_ptr<google::protobuf::Message>(prototype->New());
}

// 将 unique_ptr 转换为 shared_ptr
std::shared_ptr<google::protobuf::Message> MsgFactory::makeSharedMessage(
    std::unique_ptr<google::protobuf::Message> msg)
{
    return std::shared_ptr<google::protobuf::Message>(std::move(msg));
}
