#pragma once
#include "noncopyable.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : public noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();

    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    // 如果工作在多线程中，baseLoop_默认以轮询的方式分配channel给subLoop
    EventLoop *getNextLoop();

    std::vector<EventLoop *> getAllLoops();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }
    bool started() const { return started_; }
    std::string name() const { return name_; }

private:
    EventLoop *baseLoop_; // EventLoop loop;
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_; // 创建的所有的线程
    std::vector<EventLoop *> loops_;                         // 上面线程里面事件循环的指针
};