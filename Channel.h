#pragma once

#include <functional>
#include <memory>

#include "noncopyable.h"
#include "TimeStamp.h"

//  理清楚 EventLoop、channel、Poller之间的关系 <==== Reactor模型师对应于Demultiplex
//  Channel理解为通道，封装了sockfd和感兴趣的event，如EPOLLIN、EPOLLOUT事件
//  还绑定了poller返回的具体事件
class EventLoop;

class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallBack = std::function<void(TimeStamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // 处理事件：得到poller通知以后，调用事件对应的回调函数
    void handleEvent(TimeStamp receiveTime);

    // 设置回调函数对象
    void setReadCallback(ReadEventCallBack cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // 防止channel被手动被remove掉之后，channel还在执行回调函数
    void tie(const std::shared_ptr<void> &);
    int fd() const { return fd_; }
    int events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; }

    // 设置fd相应的事件状态
    void enableReading()
    {
        events_ |= kReadEvent;
        update();
    }

    void disableReading()
    {
        events_ &= ~kReadEvent;
        update();
    }

    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    }

    void disableWriting()
    {
        events_ &= ~kWriteEvent;
        update();
    }

    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }

    // 返回当前事件的状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // one loop peer thread
    EventLoop *ownerLoop() { return loop_; }
    void remove();
private:
    void update();
    void handleEventWithGuard(TimeStamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;
    EventLoop *loop_; // 事件循环
    const int fd_;    // poller监听的对象
    int events_;      // 注册fd感兴趣的事件
    int revents_;     // poller返回的具体发生的事件
    int index_;       // index对应于三个状态：kNew、kAdded和kDelete，见EPoller.cc文件

    std::weak_ptr<void> tie_; // 监听eventloop中是否remove掉了channel
    bool tied_;

    // channel能获知fd最终发生的具体事件的events，所以它负责调用具体事件的回调函数
    ReadEventCallBack readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_; // 由TcpConnection对其进行调用赋值
    EventCallback errorCallback_;
};