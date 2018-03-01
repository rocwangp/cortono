#pragma once

#include <memory>
#include <atomic>

#include "poller.h"

namespace cortono
{
    namespace net
    {

        class EventLoop
        {
            public:
                EventLoop()
                    : quit_(false),
                      poller_(std::make_shared<Poller>())
                {

                }

                void quit()
                {
                    quit_.store(true);
                }

                void sync_loop()
                {
                    while(!quit_)
                    {
                        poller_->wait();
                    }
                }

                auto poller() { return poller_; }
            private:
                std::atomic<bool> quit_;
                std::shared_ptr<Poller> poller_;

        };
    }
}


