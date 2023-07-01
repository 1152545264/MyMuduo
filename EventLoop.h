#pragma once
#include "Channel.h"
#include "noncopyable.h"
#include "TimeStamp.h"
#include "CurrentThread.h"

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

// 前置声明
class Channel;
class Poller;

// 事件循环类 主要包含了两个模块Channel 和Poller（epoll的抽象） 一个线程一个Loop
class EventLoop : public noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();
    // 退出事件循环
    void quit();

    // 在当前loop中循环cb，供运行loop所在线程中的函数调用
    TimeStamp pollReturnTime() const { return pollReturnTime_; }
    void runInLoop(Functor cb);
    // 把cb放入队列中，唤醒loop所在的线程，执行cb，供运行在和loop所在线程不同的线程种的函数调用
    void queueInLoop(Functor cb);

    // 用来唤醒loop所在的线程
    void wakeUp();

    // EventLoop的方法 ===> Poller的方法
    void updateChannel(Channel *ch);
    void removeChannel(Channel *ch);
    bool hashChannel(Channel *ch);

    // 判断EventLoop对象是否在自己的线程里面
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }



private:
    void handleRead();        // 唤醒
    void doPendingFunctors(); // 执行回调

    using ChannelList = std::vector<Channel *>;
    std::atomic_bool looping_; // 原子操作，通过CAS实现的
    std::atomic_bool quit_;    // 标识退出loop循环
    const pid_t threadId_;     // 记录当前loop所在线程的id
    TimeStamp pollReturnTime_; // poller返回发生事件的channels的时间点
    std::unique_ptr<Poller> poller_;

    // wakeUpFd_主要作用：当mainLoop获取一个新用户的channel，通过轮询算法选择一个subloop，通过该成员唤醒subloop处理channel
    // 由Linux中比较新的系统调用eventfd创建wakeupFd_
    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_; // 封装wakeupFd_以及其感兴趣的事件，完成主loop唤醒workLoop


    ChannelList activeChannels_;    // eventLoop所管理的所有channel
    Channel *currentActiveChannel_; // 有无currentActiveChannel_影响不大

    std::atomic_bool callingPendingFunctors_; // 标识当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingsFunctors_;    // 存储loop需要执行的所有回调操作
    std::mutex mutex_;                        // 互斥锁用来保护上面vector容器的线程安全操作
};
