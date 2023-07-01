#pragma once

#include "noncopyable.h"
#include "Thread.h"
#include "EventLoop.h"

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>


class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop *startLoop();

private:
    void threadFunc(); // 在这个线程函数中创建loop

    EventLoop *loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_; //线程初始化需要的回调函数
};