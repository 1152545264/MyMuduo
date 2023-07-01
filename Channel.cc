#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{
}

Channel::~Channel()
{
}

// 一个TcpConnection创建的时候TcpConnection  => channel
//  防止channel被手动被remove掉之后，channel还在执行回调函数
// 调用处：TcpConnection::connectEstablished
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}

/************
 * 改变channel所表示的fd的events事件之后，update负责在poller里面更改fd相应的事件epoll_ctl
 * EventLoop => ChannelList Poller
 * **********/
void Channel::update()
{
    // 通过channel所属的eventloop调用poller的相应方法，注册fd的events事件
    // TODO add_code...
    loop_->updateChannel(this);
}

// 在channel所属的EventLoop中，把当前的channel删除掉
void Channel::remove()
{
    // add_code...
    loop_->removeChannel(this);
}

// 处理事件：得到poller通知以后，调用事件对应的回调函数
void Channel::handleEvent(TimeStamp receiveTime)
{
    if (tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard) // 被绑定的对象还未失效
        {
            handleEventWithGuard(receiveTime);
        }
        // 提升失败不做任何回调操作，说明绑定的TcpConnection对象已经失效
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}

// 根据epoller通知的channel发生的具体事件，由channel负责调用具体的回调函数
void Channel::handleEventWithGuard(TimeStamp receiveTime)
{
    LOG_INFO("channel handleEvents revents:%d\n", revents_);
    // 发生异常了
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
    }

    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }

    if (revents_ & (EPOLLIN | EPOLLPRI)) // 可读事件
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }
    if (revents_ & EPOLLOUT) // 可写事件
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}