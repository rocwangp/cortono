#pragma once

#include "../std.hpp"
#include "../util/util.hpp"

namespace cortono::net
{
    class timer
    {
        public:
            using time_point = std::chrono::steady_clock::time_point;
            using milliseconds = std::chrono::milliseconds;
            using seconds = std::chrono::seconds;

            timer(time_point point, std::function<void()> cb)
                : timer(point, milliseconds(0), cb)
            {
            }

            timer(time_point point, milliseconds interval, std::function<void()> cb)
                : periodic_(interval != milliseconds(0)),
                  expires_time_(std::move(point)),
                  interval_(std::move(interval)),
                  cb_(std::move(cb))
            {
            }

            timer(const timer& t)
                : periodic_(t.periodic_),
                  expires_time_(t.expires_time_),
                  interval_(t.interval_),
                  cb_(t.cb_)
            {
            }

            timer(timer&& t)
                : periodic_(std::move(t.periodic_)),
                  expires_time_(std::move(t.expires_time_)),
                  interval_(std::move(t.interval_)),
                  cb_(std::move(t.cb_))
            {
            }

            bool is_expires() const {
                return expires_time_ <= now();
            }
            bool is_periodic() const {
                return periodic_;
            }
            void run() {
                util::exitif(cb_ == nullptr, "timer callback is nullptr");
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
        private:
            bool periodic_;
            time_point expires_time_;
            milliseconds interval_;
            std::function<void()> cb_;
    };
}