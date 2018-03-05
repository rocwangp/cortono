#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <future>
#include <atomic>
#include <memory>
#include <mutex>
#include <functional>
#include <condition_variable>

#include "noncopyable.h"

namespace cortono::util
{
    class threadpool : private util::noncopyable
    {
        public:
            threadpool()
            {  }

            ~threadpool()
            { }

            void start(int thread_nums = std::thread::hardware_concurrency()) {
                for(int i = 0; i < thread_nums; ++i) {
                    threads_.emplace_back([this] {
                        while(!quit_) {
                            std::function<void()> task;
                            {
                                std::unique_lock lock { mutex_ };
                                cond_.wait(lock, [this] { return quit_ || !tasks_.empty(); });
                                if(quit_)   return;
                                task = tasks_.front();
                                tasks_.pop();
                            }
                            task();
                        }
                    });
                }
            }
            void quit() {
                quit_ = true;
                cond_.notify_all();
                for(auto& th : threads_)
                    th.join();
            }

            template <class F, class... Args>
            auto async(F&& f, Args... args)
                -> std::future<typename std::result_of<F(Args...)>::type>
            {
                using return_type = typename std::result_of<F(Args...)>::type;
                auto task = std::make_shared<std::packaged_task<return_type()>>(
                            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
                        );
                std::future result { task->get_future() };
                {
                    std::unique_lock lock { mutex_ };
                    tasks_.emplace( [task] { return (*task)(); });
                    cond_.notify_one();
                }
                return result;
            }
        private:
            std::atomic<bool> quit_;
            std::mutex mutex_;
            std::condition_variable cond_;
            std::vector<std::thread> threads_;
            std::queue<std::function<void()>> tasks_;

        public:
            static threadpool& instance()
            {
                static threadpool inst;
                return inst;
            }
    };
}
