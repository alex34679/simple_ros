#include "node_handle.h"
#include "global_init.h"
#include <muduo/base/Logging.h>
#include "timer.h"  // 添加timer头文件

NodeHandle::NodeHandle() {
    // 从SystemManager获取节点信息
    nodeInfo_ = SystemManager::instance().getNodeInfo();
    LOG_INFO << "NodeHandle initialized with node_name: " << nodeInfo_.node_name()
             << ", IP: " << nodeInfo_.ip() << ", Port: " << nodeInfo_.port();
}

NodeHandle::~NodeHandle() {
    LOG_INFO << "NodeHandle destroyed";
}

NodeHandle::NodeHandle(NodeHandle&&) noexcept = default;

NodeHandle& NodeHandle::operator=(NodeHandle&&) noexcept = default;

// 添加createTimer方法的实现
std::shared_ptr<Timer> NodeHandle::createTimer(double period, const TimerCallback& callback, bool oneshot) {
    // 获取SystemManager的事件循环
    muduo::net::EventLoop* loop = SystemManager::instance().getEventLoop().get();
    
    // 创建定时器
    auto timer = std::make_shared<Timer>(loop, period, callback);
    timer->setOneShot(oneshot);
    timer->start();
    
    return timer;
}

std::shared_ptr<Subscriber> NodeHandle::subscribe(const std::string& topic, 
                                                 uint32_t queue_size, 
                                                 const std::string& msg_type_name, 
                                                 MessageQueue::Callback callback) {
    LOG_INFO << "Subscribe to topic=" << topic << " with dynamic type=" << msg_type_name;
    
    // 创建订阅者实例
    auto subscriber = std::make_shared<Subscriber>(topic, queue_size, callback);
    
    // 调用RPC订阅
    auto rpc_client = SystemManager::instance().getRpcClient();
    if (rpc_client) {
        SubscribeResponse response;
        bool success = rpc_client->Subscribe(topic, msg_type_name, nodeInfo_, &response);
        if (success) {
            LOG_INFO << "Subscribe RPC successful for topic: " << topic << " with type: " << msg_type_name;
        } else {
            LOG_ERROR << "Subscribe RPC failed for topic: " << topic << " with type: " << msg_type_name;
        }
    } else {
        LOG_ERROR << "Global RPC client not initialized";
    }
    
    return subscriber;
}
