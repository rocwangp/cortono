#pragma once

#include "../std.hpp"
#include "../util/util.hpp"
#include "../util/noncopyable.hpp"

namespace cortono::net
{
    class event_poller : private util::noncopyable
    {

        public:
            struct event_cb
            {
                std::function<void()> read_cb, write_cb, close_cb;
            };

            enum
            {
                none_event = 0,
                read_event = EPOLLIN | EPOLLET,
                write_event = EPOLLOUT | EPOLLET
            };

        public:
            event_poller()
                : epollfd_(::epoll_create1(::EPOLL_CLOEXEC)),
                  event_nums_(0),
                  events_(1000)
            {

            }

            ~event_poller() {
                util::io::close(epollfd_);
            }

            void update(int fd, uint32_t old_events, uint32_t new_events, std::shared_ptr<event_cb> poller_cbs) {
                int epoll_opt = EPOLL_CTL_ADD;
                if(new_events != none_event) {
                    if(old_events != none_event)
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
                /* util::exitif(n == -1, std::strerror(errno), epollfd_, event_nums_); */
                for(int i = 0; i < n; ++i) {
                    if(readable_event(events_[i].events))
                        static_cast<event_cb*>(events_[i].data.ptr)->read_cb();
                    else if(writeable_event(events_[i].events))
                        static_cast<event_cb*>(events_[i].data.ptr)->write_cb();
                    else
                        static_cast<event_cb*>(events_[i].data.ptr)->close_cb();
                }
            }


        private:
            bool readable_event(uint32_t events) {
                return events & read_event;
            }

            bool writeable_event(uint32_t events) {
                return events & write_event;
            }

        private:
            int epollfd_;
            int event_nums_;
            std::vector<struct epoll_event> events_;

    };
}
