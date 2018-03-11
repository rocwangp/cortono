#pragma once

#include "watcher.hpp"
#include "poller.hpp"
#include "socket.hpp"
#include "timer.hpp"
#include "../std.hpp"
#include "../util/noncopyable.hpp"

namespace cortono::net
{
    class event_loop : private util::noncopyable
    {
        public:
            event_loop()
                : tid_(std::this_thread::get_id()),
                  quit_(false),
                  poller_(std::make_shared<event_poller>()),
                  watcher_(std::make_shared<watcher>()),
                  watch_socket_(std::make_shared<tcp_socket>(watcher_->read_fd()))
            {
                watch_socket_->tie(poller_);
                watch_socket_->enable_read([this]{ watcher_->clear(); });
            }

            void quit() {
                quit_.store(true);
            }
            void sync_loop() {
                while(!quit_) {
                    int timeout = timers_.empty() ? -1 : timers_.top().expires_milliseconds();
                    poller_->wait(timeout);
                    handle_pending_func();
                    handle_time_func();
                }
            }
            void handle_pending_func() {
                for(auto&& cb : pending_functors_) {
                    cb();
                }
            }
            void handle_time_func() {
                while(!timers_.empty() && timers_.top().is_expires()) {
                    auto t = timers_.top();
                    timers_.pop();
                    t.run();
                    if(t.is_periodic()) {
                        t.update_time();
                        timers_.emplace(std::move(t));
                    }
                }
            }
            void safe_call(std::function<void()> cb) {
                if(std::this_thread::get_id() == tid_) {
                    cb();
                }
                else {
                    {
                        std::unique_lock lock { mutex_ };
                        pending_functors_.emplace_back(std::move(cb));
                    }
                    wake_up();
                }
            }
            void wake_up() {
                watcher_->notify();
            }
            auto poller() {
                return poller_;
            }

            void set_timer(timer::time_point&& point, timer::milliseconds&& interval, std::function<void()>&& cb) {
                timers_.emplace(std::move(point), std::move(interval), std::move(cb));
            }
            void runAt(timer::time_point point, std::function<void()> cb) {
                timer::milliseconds interval{0};
                set_timer(std::move(point), std::move(interval), std::move(cb));
            }
            void runAt(timer::time_point point, timer::milliseconds interval, std::function<void()> cb) {
                set_timer(std::move(point), std::move(interval), std::move(cb));
            }
            void runAfter(timer::milliseconds interval, std::function<void()> cb) {
                runAt(timer::now() + interval, cb);
            }
            void runAfter(timer::milliseconds interval1, timer::milliseconds interval2, std::function<void()> cb) {
                runAt(timer::now() + interval1, interval2, cb);
            }
            void runEvery(timer::milliseconds interval, std::function<void()> cb) {
                runAfter(interval, interval, cb);
            }
        private:
            std::thread::id tid_;
            std::mutex mutex_;
            std::atomic_bool quit_;
            std::shared_ptr<event_poller> poller_;
            std::shared_ptr<watcher> watcher_;
            std::shared_ptr<tcp_socket> watch_socket_;
            std::vector<std::function<void()>> pending_functors_;
            std::priority_queue<timer> timers_;

    };
}


