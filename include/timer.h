// timer.h
// 实现类似ROS的定时器功能

#ifndef simple_ros_TIMER_H
#define simple_ros_TIMER_H

#include <functional>
#include <memory>
#include "muduo/net/TimerId.h"
#include "muduo/net/EventLoop.h"
#include <cmath>

namespace simple_ros {

// TimerEvent类，类似于ROS的TimerEvent，包含定时器事件信息
struct TimerEvent {
    double current_real;    // 当前时间
    double last_real;       // 上一次触发时间
    double expected_real;   // 期望触发时间
    int32_t last_duration;  // 上一次回调执行时间（毫秒）
};

// 定时器回调类型
typedef std::function<void(const TimerEvent&)> TimerCallback;

class Timer {
public:
    /**
     * @brief 构造函数
     * @param loop 事件循环
     * @param period 定时器周期（秒）
     * @param callback 回调函数
     */
    Timer(muduo::net::EventLoop* loop, double period, const TimerCallback& callback);

    /**
     * @brief 析构函数，会自动停止定时器
     */
    ~Timer();

    // 禁止拷贝和移动
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(Timer&&) = delete;

    /**
     * @brief 启动定时器
     */
    void start();

    /**
     * @brief 停止定时器
     */
    void stop();

    /**
     * @brief 暂停定时器
     */
    void pause();

    /**
     * @brief 恢复定时器
     */
    void resume();

    /**
     * @brief 设置是否为一次性定时器
     * @param oneshot true表示一次性定时器，false表示周期性定时器
     */
    void setOneShot(bool oneshot);

    /**
     * @brief 获取定时器周期
     * @return 周期（秒）
     */
    double getPeriod() const;

    /**
     * @brief 设置定时器周期
     * @param period 周期（秒）
     */
    void setPeriod(double period);

private:
    /**
     * @brief 内部回调函数，会调用用户提供的回调函数
     */
    void internalCallback();

    muduo::net::EventLoop* loop_;    // 事件循环
    double period_;                  // 定时器周期（秒）
    TimerCallback callback_;         // 用户回调函数
    muduo::net::TimerId timerId_;    // muduo定时器ID
    bool isRunning_;                 // 定时器是否运行
    bool isOneShot_;                 // 是否为一次性定时器
    bool isPaused_;                  // 是否暂停
    TimerEvent lastEvent_;           // 上一次事件信息
};

} // namespace simple_ros

#endif // simple_ros_TIMER_H