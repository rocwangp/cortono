#pragma once

#include "../std.hpp"
#include "../util/util.hpp"
#include "../util/noncopyable.hpp"

namespace cortono::net
{
    class EventPoller : private util::noncopyable
    {

        public:
            struct PollerCB
            {
                PollerCB() { clear(); }
                void clear() { read_cb = write_cb = close_cb = nullptr; }
                std::function<void()> read_cb, write_cb, close_cb;
            };

            enum
            {
                NONE_EVENT = 0,
                READ_EVENT = EPOLLIN ,//| EPOLLET,
                WRITE_EVENT = EPOLLOUT,// | EPOLLET
            };

        public:
            EventPoller()
                : epollfd_(::epoll_create1(::EPOLL_CLOEXEC)),
                  event_nums_(0),
                  events_(1000)
            {

            }

            ~EventPoller() {
                util::io::close(epollfd_);
            }

            void update(int fd, uint32_t old_events, uint32_t new_events, std::shared_ptr<PollerCB> poller_cbs) {
                int epoll_opt = EPOLL_CTL_ADD;
                if(new_events != NONE_EVENT) {
                    if(old_events != NONE_EVENT)
                        epoll_opt = EPOLL_CTL_MOD;
                    else
                        ++event_nums_;
                }
                else {
                    epoll_opt = EPOLL_CTL_DEL;
                    --event_nums_;
                }
                struct epoll_event event;
                event.events = new_events;
                event.data.ptr = poller_cbs.get();
                ::epoll_ctl(epollfd_, epoll_opt, fd, &event);
            }

            void wait(int timeout = -1)
            {
                if(event_nums_ > static_cast<int>(events_.size()))
                    events_.resize(event_nums_);
                int n = ::epoll_wait(epollfd_, &events_[0], events_.size(), timeout);
                for(int i = 0; i < n; ++i) {
                    bool io_event = false;
                    if(readable_event(events_[i].events) && events_[i].data.ptr != nullptr) {
                        static_cast<PollerCB*>(events_[i].data.ptr)->read_cb();
                        io_event = true;
                    }
                    else if(writeable_event(events_[i].events) && events_[i].data.ptr != nullptr) {
                        static_cast<PollerCB*>(events_[i].data.ptr)->write_cb();
                        io_event = true;
                    }
                    else if(!io_event && events_[i].data.ptr != nullptr) {
                        static_cast<PollerCB*>(events_[i].data.ptr)->close_cb();
                    }
                }
            }


        private:
            bool readable_event(uint32_t events) {
                return events & READ_EVENT;
            }

            bool writeable_event(uint32_t events) {
                return events & WRITE_EVENT;
            }

        private:
            int epollfd_;
            int event_nums_;
            std::vector<struct epoll_event> events_;

    };
}
