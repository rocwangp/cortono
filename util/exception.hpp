#pragma once

#include "../std.hpp"

namespace cortono::util
{
    class ret_call
    {
        public:
            typedef std::function<void()> callback;

            ret_call(callback cb)
                : cb_(std::move(cb))
            {  }

            ~ret_call() {
                if(cb_) {
                    cb_();
                }
            }
        private:
            callback cb_;
    };
}
