#pragma once

#include "../cortono.hpp"
#include <iostream>

namespace cortono::coroutine
{
    coroutine::Channel<std::function<int(int)>> channel;


    void product() {
        for(int i = 0; i < 2; ++i) {
            channel.push([](coroutine::routine_t id){
                /* std::this_thread::sleep_for(std::chrono::milliseconds(10)); */
                std::cout << "in routine func " << id << std::endl;
                coroutine::resume(id);
                return 0;
            });
        }
    }
    void routine_func() {
        /* for(int i = 0; i < 3; ++i) { */
            std::function<int(int)> task = channel.pop();
            coroutine::await(task, coroutine::current());
        /* } */
    }
    void consumer() {
        auto c1 = coroutine::create(routine_func);
        auto c2 = coroutine::create(routine_func);
        coroutine::resume(c1);
        coroutine::resume(c2);
        log_debug(c1, c2);
    }
    void test() {
        consumer();
        product();
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
