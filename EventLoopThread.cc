#include "EventLoopThread.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr),
      exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(),
      callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

// startLoop函数在等待EventLoopThread::threadFunc函数中完成，二者在不同线程中运行，属于线程之间的同步
EventLoop *EventLoopThread::startLoop()
{
    thread_.start(); // 启动底层的新线程，调用thread_在构造函数中传入的值：EventLoopThread::threadFunc
    EventLoop *loop = nullptr;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr) // 使用while循环避免虚假唤醒
        {
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}

// EventLoopThread::threadFunc是在单独的新线程里面运行的,和startLoop函数不在同一个线程
void EventLoopThread::threadFunc()
{
    // 原始代码是在此处创建了一个栈上的对象
     EventLoop loop; // 创建一个独立的Eventloop，和上面的线程是一一对应的，one loop per thread
     //fixme 引用栈上创建的对象地址是否会导致该对向自动被释放后非法访问?
     //不会导致非法内存访问， 因为该线程函数会阻塞在下面的loop.loop()函数处，除非loop循环终止，才有threadFunc()才有可能返回
     if (callback_)
     {
         callback_(&loop); // 针对loop做一些初始化工作
     }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop(); // EventLoop::loop ===>Poller::poll() (此处会阻塞等待，loop函数里面有个基本上算是死循环的循环)

    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}