// timer.cc
// Timer类的实现

#include "timer.h"
#include "muduo/base/Timestamp.h"
#include <thread>

namespace simple_ros {

Timer::Timer(muduo::net::EventLoop* loop, double period, const TimerCallback& callback)
    : loop_(loop),
      period_(period),
      callback_(callback),
      isRunning_(false),
      isOneShot_(false),
      isPaused_(false) {
    lastEvent_.current_real = 0.0;
    lastEvent_.last_real = 0.0;
    lastEvent_.expected_real = 0.0;
    lastEvent_.last_duration = 0;
}

Timer::~Timer() {
    stop();
}

void Timer::start() {
    if (isRunning_ || isPaused_) return;
    
    isRunning_ = true;
    isPaused_ = false;
    
    // 获取当前时间作为期望触发时间的基准
    lastEvent_.expected_real = muduo::Timestamp::now().secondsSinceEpoch();
    
    if (isOneShot_) {
        timerId_ = loop_->runAfter(period_, std::bind(&Timer::internalCallback, this));
    } else {
        timerId_ = loop_->runEvery(period_, std::bind(&Timer::internalCallback, this));
    }
}

void Timer::stop() {
    if (!isRunning_ && !isPaused_) return;
    
    if (isRunning_) {
        loop_->cancel(timerId_);
    }
    
    isRunning_ = false;
    isPaused_ = false;
}

void Timer::pause() {
    if (!isRunning_ || isPaused_) return;
    
    loop_->cancel(timerId_);
    isRunning_ = false;
    isPaused_ = true;
}

void Timer::resume() {
    if (isRunning_ || !isPaused_) return;
    
    isRunning_ = true;
    isPaused_ = false;
    
    // 计算剩余时间
    double currentTime = muduo::Timestamp::now().secondsSinceEpoch();
    double elapsed = currentTime - lastEvent_.current_real;
    double remaining = period_ - std::fmod(elapsed, period_);
    
    if (remaining < 0) remaining = 0;
    
    if (isOneShot_) {
        timerId_ = loop_->runAfter(remaining, std::bind(&Timer::internalCallback, this));
    } else {
        timerId_ = loop_->runEvery(period_, std::bind(&Timer::internalCallback, this));
    }
}

void Timer::setOneShot(bool oneshot) {
    bool wasRunning = isRunning_;
    if (wasRunning) {
        stop();
    }
    
    isOneShot_ = oneshot;
    
    if (wasRunning) {
        start();
    }
}

void Timer::setPeriod(double period) {
    bool wasRunning = isRunning_;
    if (wasRunning) {
        stop();
    }
    
    period_ = period;
    
    if (wasRunning) {
        start();
    }
}

double Timer::getPeriod() const {
    return period_;
}

void Timer::internalCallback() {
    // 记录当前时间
    double startTime = muduo::Timestamp::now().secondsSinceEpoch();
    
    // 更新事件信息
    TimerEvent event;
    event.current_real = startTime;
    event.last_real = lastEvent_.current_real;
    event.expected_real = lastEvent_.expected_real + period_;
    event.last_duration = lastEvent_.last_duration;
    
    // 调用用户回调
    try {
        if (callback_) {
            callback_(event);
        }
    } catch (const std::exception& e) {
        // 实际应用中应该有更好的异常处理
        fprintf(stderr, "Timer callback exception: %s\n", e.what());
    }
    
    // 计算回调执行时间
    double endTime = muduo::Timestamp::now().secondsSinceEpoch();
    lastEvent_.last_duration = static_cast<int32_t>((endTime - startTime) * 1000);
    lastEvent_.current_real = startTime;
    lastEvent_.expected_real = event.expected_real;
    
    // 对于一次性定时器，执行后停止
    if (isOneShot_) {
        isRunning_ = false;
    }
}

} // namespace simple_ros