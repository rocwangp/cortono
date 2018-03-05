#pragma once

#include <memory>
#include <atomic>
#include <queue>
#include <thread>
#include <mutex>

#include "watcher.hpp"
#include "poller.hpp"
#include "socket.hpp"
#include "../util/noncopyable.hpp"

namespace cortono::net
{
    class cort_eventloop : private util::noncopyable
    {
        public:
            cort_eventloop()
                : tid_(std::this_thread::get_id()),
                  quit_(false),
                  poller_(std::make_shared<cort_poller>()),
                  watcher_(std::make_shared<watcher>()),
                  watch_socket_(std::make_shared<cort_socket>(watcher_->read_fd()))
            {
                watch_socket_->tie(poller_);
                watch_socket_->enable_read([this]{ watcher_->clear(); });
            }

            void quit() {
                quit_.store(true);
            }

            void sync_loop() {
                while(!quit_) {
                    poller_->wait();
                    for(auto&& cb: pending_functors_) {
                        cb();
                    }
                    pending_functors_.clear();
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
        private:
            std::thread::id tid_;
            std::mutex mutex_;
            std::atomic_bool quit_;
            std::shared_ptr<cort_poller> poller_;
            std::shared_ptr<watcher> watcher_;
            std::shared_ptr<cort_socket> watch_socket_;
            std::vector<std::function<void()>> pending_functors_;

    };
}


