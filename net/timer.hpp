#pragma once

#include "../std.hpp"
#include "../util/util.hpp"

namespace cortono::net
{
    class Timer
    {
        public:
            using time_point = std::chrono::steady_clock::time_point;
            using milliseconds = std::chrono::milliseconds;
            using seconds = std::chrono::seconds;
            using timer_id = std::uint64_t;

            Timer() {}
            Timer(time_point point, std::function<void()> cb)
                : Timer(point, milliseconds(0), cb)
            {
            }

            Timer(time_point point, milliseconds interval, std::function<void()> cb)
                : periodic_(interval != milliseconds(0)),
                  expires_time_(std::move(point)),
                  interval_(std::move(interval)),
                  cb_(std::move(cb)),
                  id_(timer_count++)
            {
            }

            Timer(const Timer& t)
                : periodic_(t.periodic_),
                  expires_time_(t.expires_time_),
                  interval_(t.interval_),
                  cb_(t.cb_),
                  id_(t.id_)
            {
            }

            Timer(Timer&& t)
                : periodic_(std::move(t.periodic_)),
                  expires_time_(std::move(t.expires_time_)),
                  interval_(std::move(t.interval_)),
                  cb_(std::move(t.cb_)),
                  id_(std::move(t.id_))
            {
            }

            Timer& operator=(const Timer& t) {
                if(this != &t) {
                    Timer tmp(t);
                    std::swap(*this, tmp);
                }
                return *this;
            }
            Timer& operator=(Timer&& t) {
                if(this != &t) {
                    periodic_ = std::move(t.periodic_);
                    expires_time_ = std::move(t.expires_time_);
                    interval_ = std::move(t.interval_);
                    cb_ = std::move(t.cb_);
                    id_ = std::move(t.id_);
                }
                return *this;
            }
            timer_id id() const {
                return id_;
            }
            bool is_expires() const {
                return expires_time_ <= now();
            }
            bool is_periodic() const {
                return periodic_;
            }
            void run() {
                exitif(cb_ == nullptr, "timer callback is nullptr");
                cb_();
            }
            int expires_milliseconds() const {
                return static_cast<int>(std::chrono::duration_cast<milliseconds>(
                    expires_time_ - now()).count());
            }
            int expires_seconds() const {
                return static_cast<int>(std::chrono::duration_cast<seconds>(
                    expires_time_ - now()).count());
            }
            void update_time() {
                expires_time_ += interval_;
            }
            static time_point now() {
                return std::chrono::steady_clock::now();
            }

            bool operator==(const Timer& t) const {
                return expires_time_ == t.expires_time_;
            }
            bool operator!=(const Timer& t) const {
                return !operator==(t);
            }
            bool operator>(const Timer& t) const {
                return expires_time_ > t.expires_time_;
            }
            bool operator<(const Timer& t) const{
                return expires_time_ < t.expires_time_;
            }
        private:
            bool periodic_;
            time_point expires_time_;
            milliseconds interval_;
            std::function<void()> cb_;
            std::uint64_t id_;

            static std::uint64_t timer_count;
    };
    inline std::uint64_t Timer::timer_count = 0;
}
