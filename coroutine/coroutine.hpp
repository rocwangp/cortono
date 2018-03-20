#pragma once
#include "../std.hpp"
#include "../cortono.hpp"
#include <ucontext.h>
#include <unistd.h>
#include <iostream>

namespace cortono::coroutine
{
    struct Routine
    {

        std::function<void()> func;
        char* st;
        bool finished;
        ucontext_t ctx;

        Routine(std::function<void()> f)
            : func(std::move(f)),
              st(nullptr),
              finished(false)
        {  }

        ~Routine() {
            if(st) delete[] st;
        }
    };

    typedef int routine_t;

    struct Ordinator
    {
        std::vector<Routine*> routines;
        std::list<routine_t> indexes;
        routine_t current;
        std::size_t stack_size;
        ucontext_t main_ctx;

        enum { STACK_LIMIT = 128 * 1024 };
        Ordinator(std::size_t ss = STACK_LIMIT)
            : current(-1),
              stack_size(ss)
        {  }

        ~Ordinator() {
            for(auto& p : routines)
                delete p;
        }
    };

    thread_local static Ordinator ordinator;

    inline routine_t create(std::function<void()> f) {
        auto routine = new Routine(f);
        if(ordinator.indexes.empty()) {
            ordinator.routines.emplace_back(routine);
            return ordinator.routines.size() - 1;
        }
        else {
            auto id = ordinator.indexes.front();
            ordinator.indexes.pop_front();
            assert(ordinator.routines[id] == nullptr);
            ordinator.routines[id] = routine;
            return id;
        }
    }
    inline void destroy(routine_t id) {
        assert(ordinator.routines[id] != nullptr);
        delete ordinator.routines[id];
        ordinator.routines[id] = nullptr;
    }
    inline void entry() {
        routine_t id = ordinator.current;
        assert(ordinator.routines[id] != nullptr);
        auto routine = ordinator.routines[id];
        routine->func();

        routine->finished = true;
        ordinator.current = -1;
        ordinator.indexes.push_back(id);
    }
    inline int resume(routine_t id) {
        auto routine = ordinator.routines[id];
        if(routine->st == nullptr) {
            ::getcontext(&routine->ctx);
            routine->st = new char[ordinator.stack_size];
            routine->ctx.uc_stack.ss_sp = routine->st;
            routine->ctx.uc_stack.ss_size = ordinator.stack_size;
            routine->ctx.uc_link = &ordinator.main_ctx;
            ordinator.current = id;
            ::makecontext(&routine->ctx, (void(*)(void))entry, 0);
            ::swapcontext(&ordinator.main_ctx, &routine->ctx);
        }
        else {
            ordinator.current = id;
            ::swapcontext(&ordinator.main_ctx, &routine->ctx);
        }
        return 0;
    }
    inline void yield() {
        routine_t id = ordinator.current;
        auto routine = ordinator.routines[id];
        assert(routine != nullptr);

        char *stack_top = routine->st + ordinator.stack_size;
        char stack_bottom = 0;
        assert(static_cast<std::size_t>(stack_top - &stack_bottom) <= ordinator.stack_size);

        ordinator.current = -1;
        ::swapcontext(&routine->ctx, &ordinator.main_ctx);
    }
    inline routine_t current() {
        return ordinator.current;
    }
    template <class Function, class... Args>
    auto await(Function&& f, Args&&... args)
        -> typename std::result_of<Function(Args...)>::type
    {
        auto fu = std::async(std::launch::async, std::forward<Function>(f), std::forward<Args>(args)...);
        std::future_status status = fu.wait_for(std::chrono::milliseconds(0));
        while(status == std::future_status::timeout) {
            if(ordinator.current != -1)
                yield();
            status = fu.wait_for(std::chrono::milliseconds(0));
        }
        return fu.get();
    }

    template <typename T>
    class Channel
    {
        public:
            void push(const T& task) {
                tasks_.emplace(task);
                if(!takers_.empty()) {
                    auto taker = takers_.front();
                    takers_.pop();
                    resume(taker);
                }
            }
            T pop() {
                if(tasks_.empty()) {
                    takers_.emplace(current());
                }
                while(tasks_.empty())
                    yield();
                auto task = std::move(tasks_.front());
                tasks_.pop();
                return task;
            }
            void clear() {
                tasks_.clear();
            }
            std::size_t size() const {
                return tasks_.size();
            }
            bool empty() const {
                return tasks_.empty();
            }
        private:
            std::queue<T> tasks_;
            std::queue<routine_t> takers_;
    };
}
