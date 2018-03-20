#pragma once

#include "watcher.hpp"
#include "poller.hpp"
#include "socket.hpp"
#include "timer.hpp"
#include "../std.hpp"
#include "../util/noncopyable.hpp"

namespace cortono::net
{
    class EventLoop : private util::noncopyable
    {
        public:
            EventLoop()
                : tid_(std::this_thread::get_id()),
                  quit_(false),
                  poller_(std::make_shared<EventPoller>()),
                  watcher_(std::make_shared<Watcher>()),
                  watch_socket_(std::make_shared<TcpSocket>(watcher_->read_fd()))
            {
                watch_socket_->tie(poller_);
                watch_socket_->enable_reading();
                watch_socket_->set_read_callback([this] { watcher_->clear(); });
            }

            void quit() {
                quit_.store(true);
            }
            void loop() {
                while(!quit_) {
                    loop_once();
                }
            }
            void loop_once() {
                int timeout = timers_.empty() ? -1 : timers_.top().expires_milliseconds();
                poller_->wait(timeout);
                handle_pending_func();
                handle_time_func();
            }
            void handle_pending_func() {
                for(auto& cb : pending_functors_) {
                    cb();
                }
                /* decltype(pending_functors_)().swap(pending_functors_); */
                pending_functors_.clear();
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
                if(std::this_thread::get_id() == tid_)
                {
                    cb();
                }
                else
                {
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

            void set_timer(Timer::time_point&& point, Timer::milliseconds&& interval, std::function<void()>&& cb) {
                timers_.emplace(std::move(point), std::move(interval), std::move(cb));
            }
            void runAt(Timer::time_point point, std::function<void()> cb) {
                Timer::milliseconds interval{0};
                set_timer(std::move(point), std::move(interval), std::move(cb));
            }
            void runAt(Timer::time_point point, Timer::milliseconds interval, std::function<void()> cb) {
                set_timer(std::move(point), std::move(interval), std::move(cb));
            }
            void runAfter(Timer::milliseconds interval, std::function<void()> cb) {
                runAt(Timer::now() + interval, cb);
            }
            void runAfter(Timer::milliseconds interval1, Timer::milliseconds interval2, std::function<void()> cb) {
                runAt(Timer::now() + interval1, interval2, cb);
            }
            void runEvery(Timer::milliseconds interval, std::function<void()> cb) {
                runAfter(interval, interval, cb);
            }
        private:
            std::thread::id tid_;
            std::mutex mutex_;
            std::atomic_bool quit_;
            std::shared_ptr<EventPoller> poller_;
            std::shared_ptr<Watcher> watcher_;
            std::shared_ptr<TcpSocket> watch_socket_;
            std::vector<std::function<void()>> pending_functors_;
            std::priority_queue<Timer> timers_;

    };
}


