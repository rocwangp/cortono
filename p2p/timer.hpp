#pragma once

#include "../cortono.hpp"

namespace p2p {

class RepeatTimer {
public:
    ~RepeatTimer() {
        disable();
    }
    void enable(cortono::net::EventLoop* loop, std::chrono::milliseconds timeout, std::function<void()> cb) {  
        loop_ = loop;
        timeout_ = std::move(timeout);
        cb_ = std::move(cb);
        timer_id_ = loop_->run_every(timeout_, cb_);
    }
    void disable() {
        if(loop_) {
            // log_info("cancel timer:", timer_id_);
            loop_->cancel_timer(timer_id_);
        }
    }
    void reset() {
        if(loop_) {
            disable();
            timer_id_ = loop_->run_every(timeout_, cb_);
        }
        else {
            log_error("repeat_timer is not initialized");
        }
    }

private:
    cortono::net::Timer::timer_id timer_id_;
    cortono::net::EventLoop* loop_{ nullptr };
    std::chrono::milliseconds timeout_{ 0 };
    std::function<void()> cb_{ nullptr };

};



};
