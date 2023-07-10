#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory>

// 防止一个线程里面创建多个EventLoop  __thread类型等价于thread_local类型
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的Poller IO复用接口超时时间
const int kPollTimeMs = 10000;

// 创建wakeupfd，用来notify唤醒subReactor处理新来的channel
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd create error: %d\n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop Created %p in thread %d \n", this, threadId_);
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread % d \n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    // 设置weakupfd的事件类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每一个eventLoop都将监听weakupchannel上的EPOLLIN事件了
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove(); // weakupChannel是一个智能指针，会自动完成删除
    close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping", this);

    while (!quit_)
    {
        activeChannels_.clear();
        // poller_ 监听两类fd：一种是clientFd，另一种是weakupFd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (auto channel : activeChannels_)
        {
            // poller监听哪些channel发生了事件，然后上报给EventLoop，通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        /**
        执行当前EventLoop事件循环需要处理的回调操作
        IO线程mainLoop (mainReactor)主要功能：accept新用户的连接并返回一个用于通信的文件描述符fd，这个fd会被打包成一个channel
        已连接的channel被分配给subReactor（subLoop）
        mainLoop事先注册一个回调cb，（需要subLoop执行） mainLoop通过eventfd唤醒subloop之后让subloop干活（下面的doPendingFunctors函数），
        执行之前mainloop注册的回调操作
        **/
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
}
// 退出事件循环两种情况:: 1 loop在自己的线程中调用quit 2 在非loop的线程中调用了loop的quit
/*************************************
 *              mainLoop
 =====================================TODO ，可以在生产者--消费者的线程安全的队列（相比较于现在更简单明了），
 =====================================不过mudup没有选择生产者和消费者这种方式，而是通过wakeupfd_来唤醒的
 * subLoop1   subLoop2  subLoop3
 * ***********************************/
void EventLoop::quit()
{
    quit_ = true;

    //如果是在其他线程中调用的quit：如在一个subLoop（worker线程）中调用了mainLoop（IO线程）\
    的quit,需要先唤醒目标loop所在的线程才能退出
    if (!isInLoopThread())
    {
        wakeUp();
    }
}

// 在当前loop中执行cb
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) // 在当前的loop线程中执行cb
    {
        cb();
    }
    else // 在非当前loop线程中执行cb，就需要唤醒loop所在线程并执行cb
    {
        queueInLoop(cb);
    }
}

// 把cb放入队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingsFunctors_.emplace_back(cb);
    }
    // 唤醒相应的 需要执行上面回调操作的loop线程了
    //||callingPendingFunctors_作用：当前loop正在执行回调，但是loop又有了新的回调，因此需要重新唤醒一次
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeUp(); // 唤醒loop所在线程
    }
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one); // 这行和下一行的代码的作用？
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %ld bytes instead of 8", n);
    }
}

// 用来唤醒loop所在的channel，向wakupfd_写一个数据,wakeupChannel_就发生读事件，当前loop所在的线程就会被唤醒
// 写什么数据和读什么数据并不重要，主要就是为了完成唤醒EventLoop所在的线程
void EventLoop::wakeUp()
{
    uint64_t one = 1;
    size_t n = write(wakeupFd_, &one  , sizeof one);
    if (n != sizeof one) //
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}



// EventLoop的方法 ===> Poller的方法
void EventLoop::updateChannel(Channel *ch)
{
    poller_->updateChannel(ch);
}

void EventLoop::removeChannel(Channel *ch)
{
    poller_->removeChannel(ch);
}

bool EventLoop::hashChannel(Channel *ch)
{
    return poller_->hasChannel(ch);
}

void EventLoop::doPendingFunctors() // 执行回调,注册回调由TCPServer类完成
{
    std::vector<Functor> functors;
	 callingPendingFunctors_ = true;
    // 下面这两句代码有助于提高系统并发度，降低框架时延
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingsFunctors_);
    }

    for (const Functor &functor : functors)
    {
        functor(); // 执行当前loop需要执行的回调操作
    }
    callingPendingFunctors_ = false;
}