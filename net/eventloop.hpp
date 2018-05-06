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
                log_info("eventloop is quiting");
                quit_.store(true);
                if(std::this_thread::get_id() != tid_) {
                    wake_up();
                }
            }
            void loop() {
                while(!quit_) {
                    loop_once();
                }
            }
            void loop_once() {
                if(!timers_.empty() && timers_.begin()->expires_milliseconds() <= 0) {
                    handle_time_func();
                }
                int timeout = timers_.empty() ? -1 : timers_.begin()->expires_milliseconds();
                poller_->wait(timeout);
                handle_pending_func();
                handle_time_func();
            }
            void handle_pending_func() {
                for(auto& cb : pending_functors_) {
                    cb();
                }
                pending_functors_.clear();
            }
            void handle_time_func() {
                while(!timers_.empty() && timers_.begin()->is_expires()) {
                    auto t = *timers_.begin();
                    timers_.erase(timers_.begin());
                    t.run();
                    if(t.is_periodic()) {
                        t.update_time();
                        timers_.emplace(std::move(t));
                        id_to_timers_[t.id()] = t;
                    }
                    else {
                        id_to_timers_.erase(t.id());
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

            Timer::timer_id set_timer(Timer::time_point&& point, Timer::milliseconds&& interval, std::function<void()>&& cb) {
                Timer timer(std::move(point), std::move(interval), std::move(cb));
                timers_.emplace(timer);
                id_to_timers_.emplace(timer.id(), timer);
                return timer.id();
            }
            Timer::timer_id run_at(Timer::time_point point, std::function<void()> cb) {
                Timer::milliseconds interval{0};
                return set_timer(std::move(point), std::move(interval), std::move(cb));
            }
            Timer::timer_id run_at(Timer::time_point point, Timer::milliseconds interval, std::function<void()> cb) {
                return set_timer(std::move(point), std::move(interval), std::move(cb));
            }
            Timer::timer_id run_after(Timer::milliseconds interval, std::function<void()> cb) {
                return run_at(Timer::now() + interval, cb);
            }
            Timer::timer_id run_after(Timer::milliseconds interval1, Timer::milliseconds interval2, std::function<void()> cb) {
                return run_at(Timer::now() + interval1, interval2, cb);
            }
            Timer::timer_id run_every(Timer::milliseconds interval, std::function<void()> cb) {
                return run_after(interval, interval, cb);
            }
            void cancel_timer(const Timer::timer_id& id) {
                if(id_to_timers_.count(id)) {
                    timers_.erase(id_to_timers_[id]);
                    id_to_timers_.erase(id);
                }
            }
        private:
            std::thread::id tid_;
            std::mutex mutex_;
            std::atomic_bool quit_;
            std::shared_ptr<EventPoller> poller_;
            std::shared_ptr<Watcher> watcher_;
            std::shared_ptr<TcpSocket> watch_socket_;
            std::vector<std::function<void()>> pending_functors_;
            /* std::priority_queue<Timer, std::vector<Timer>, std::greater<Timer>> timers_; */
            std::set<Timer> timers_;
            std::unordered_map<Timer::timer_id, Timer> id_to_timers_;
    };
}


