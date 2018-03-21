#pragma once

#include "../cortono.hpp"
#include <iostream>

namespace cortono::coroutine
{
    coroutine::Channel<std::function<int(int)>> channel;


    void product() {
        for(int i = 0; i < 10 ; ++i) {
            channel.push([](coroutine::routine_t id){
                std::cout << "in routine func " << id << std::endl;
                /* std::this_thread::sleep_for(std::chrono::milliseconds(100)); */
                /* coroutine::resume(id); */
                return 0;
            });
            /* std::cout << "in product" << std::endl; */
        }
    }
    void routine_func() {
        int n = 2;
        while(n--) {
            auto task = channel.pop();
            task(coroutine::current());
            /* coroutine::await(task, coroutine::current()); */
        }
    }
    void consumer() {
        auto c1 = coroutine::create(routine_func);
        auto c2 = coroutine::create(routine_func);
        coroutine::resume(c1);
        coroutine::resume(c2);
        /* log_debug(c1, c2); */
    }
    void test() {
        consumer();
        product();
        /* std::this_thread::sleep_for(std::chrono::seconds(1)); */
        /* std::cin.get(); */
    }
    /* void routine_func1() { */
    /*     auto n = channel.pop(); */
    /*     std::cout << "03" << std::endl; */
    /*     std::cout << n << std::endl; */
    /* } */
    /* void routine_func2() { */
    /*     auto n = channel.pop(); */
    /*     std::cout << "05" << std::endl; */
    /*     std::cout << n << std::endl; */
    /* } */

    /* void test() { */
    /*     auto c1 = coroutine::create(routine_func1); */
    /*     auto c2 = coroutine::create(routine_func2); */

    /*     std::cout << "00" << std::endl; */
    /*     coroutine::resume(c1); */

    /*     std::cout << "01" << std::endl; */
    /*     coroutine::resume(c2); */

    /*     std::cout << "02" << std::endl; */
    /*     channel.push(10); */

    /*     std::cout << "04" << std::endl; */
    /*     channel.push(11); */

    /*     coroutine::destroy(c1); */
    /*     coroutine::destroy(c2); */
    /* } */
}
