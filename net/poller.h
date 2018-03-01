#pragma once

#include <memory>
#include <vector>
#include <functional>

#include <cstdint>
#include <sys/epoll.h>

#include "../util/util.h"

namespace cortono
{
    namespace net
    {
        class Poller
        {

            public:
                struct CB
                {
                    std::function<void()> read_cb, write_cb, close_cb;
                };

                enum
                {
                    NONE_EVENT = 0,
                    READ_EVENT = EPOLLIN | EPOLLET,
                    WRITE_EVENT = EPOLLOUT | EPOLLET
                };

            public:
                Poller()
                    : epollfd_(::epoll_create1(::EPOLL_CLOEXEC)),
                      event_nums_(0),
                      events_(1000)
                {

                }

                ~Poller()
                {
                    util::io::close(epollfd_);
                }

                void update(int fd, uint32_t old_events, uint32_t new_events, std::shared_ptr<Poller::CB> poller_cbs)
                {
                    int epoll_opt = EPOLL_CTL_ADD;
                    if(new_events != NONE_EVENT)
                    {
                        log_debug;
                        if(old_events != NONE_EVENT)
                            epoll_opt = EPOLL_CTL_MOD;
                        else
                            ++event_nums_;
                    }
                    else
                    {
                        log_debug;
                        epoll_opt = EPOLL_CTL_DEL;
                        --event_nums_;
                    }
                    log_debug(event_nums_);
                    struct epoll_event event;
                    event.events = new_events;
                    event.data.ptr = poller_cbs.get();
                    ::epoll_ctl(epollfd_, epoll_opt, fd, &event);
                }

                void wait(int timeout = -1)
                {
                    if(event_nums_ > static_cast<int>(events_.size()))
                        events_.resize(event_nums_);
                    log_debug(event_nums_);
                    int n = ::epoll_wait(epollfd_, &events_[0], events_.size(), timeout);
                    util::exitif(n == -1, std::strerror(errno), epollfd_, event_nums_);
                    for(int i = 0; i < n; ++i)
                    {
                        if(read_event(events_[i].events))
                            static_cast<Poller::CB*>(events_[i].data.ptr)->read_cb();
                        else if(write_event(events_[i].events))
                            static_cast<Poller::CB*>(events_[i].data.ptr)->write_cb();
                        else
                            static_cast<Poller::CB*>(events_[i].data.ptr)->close_cb();
                    }
                }


            private:
                bool read_event(uint32_t events)
                {
                    return events & READ_EVENT;
                }

                bool write_event(uint32_t events)
                {
                    return events & WRITE_EVENT;
                }

            private:
                int epollfd_;
                int event_nums_;
                std::vector<struct epoll_event> events_;

        };
    }
}
