#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"
#include "TimeStamp.h"

#include <memory.h>
#include <unistd.h>

// channel未添加到poller中
const int kNew = -1; // channel的成员index_ = -1
// channel已经添加到poller中
const int kAdded = 1;
// channel从poller中删除
const int kDelete = 2;

EPoller::EPoller(EventLoop *loop)
    : Poller(loop),
      epollfd_(epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) // vector<epoll_event>
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d\n", errno);
    }
}

EPoller::~EPoller()
{
    ::close(epollfd_);
}

// 重写基类Poller的抽象方法
TimeStamp EPoller::poll(int timeoutMs, ChannelLists *activeChannels)
{
    // 实际上应该用LOG_DEBUG输出日志更为合理
    LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channelsMap_.size());

    // &*events_.begin() 获取vector数组首元素的地址
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    TimeStamp now(TimeStamp::now());
    if (numEvents > 0)
    {
        LOG_INFO("%d events hanppened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        // 由于此处采用的是LT模式，未处理的事件会不断上报，直到事件被处理。
        // 此处numEvent等于event_.size()时就需要对event_进行手动扩容了
        if (events_.size() == numEvents)
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0) // 没有事件发生，但是timeout超时返回了
    {
        LOG_DEBUG("%s timeout ! \n", __FUNCTION__);
    }
    else // 小于0表示发生了错误
    {
        if (saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EPoller::poll error!\n");
        }
    }
    return now;
}

// channel update remove ===> EventLoop updateChannel removeChannel ====>Poller
/**
 *                   EventLoop   ====> poller.poll
 *      ChannelList             Poller
 *                              ChannelMap <fd,Channel*>
 */
void EPoller::updateChannel(Channel *ch)
{
    const int index = ch->index(); // index对应于三个状态：kNew、kAdded和kDelete
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, ch->fd(), ch->events(), index);

    if (index == kNew || index == kDelete)
    {
        if (index == kNew)
        {
            int fd = ch->fd();
            channelsMap_[fd] = ch;
        }
        ch->set_index(kAdded);
        update(EPOLL_CTL_ADD, ch);
    }
    else // ch已经在Poller上注册过了
    {
        int fd = ch->fd();
        if (ch->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, ch);
            ch->set_index(kDelete);
        }
        else
        {
            update(EPOLL_CTL_MOD, ch);
        }
    }
}

// 从poller中删除Channel
void EPoller::removeChannel(Channel *ch)
{
    int fd = ch->fd();
    channelsMap_.erase(fd);
    LOG_INFO("func=%s => fd=%d \n", __FUNCTION__, fd);

    int index = ch->index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, ch);
    }
    ch->set_index(kNew);
}

// 填写活跃的连接
void EPoller::fillActiveChannels(int numEvents, ChannelLists *activeChannels) const
{
    for (int i = 0; i < numEvents; ++i)
    {
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        // EventLoop就拿到了它的poller给它返回的所有发生事件的Channel列表
        activeChannels->push_back(channel);
    }
}

// 更新channel通道
void EPoller::update(int operation, Channel *ch)
{
    epoll_event event;
    bzero(&event, sizeof event);

    int fd = ch->fd();

    event.events = ch->events();

    /* FIXME
    fd和ptr的赋值顺序不能换？换了会造成Segmentation fault（非法访问内存），所以为什么？注意这是一个联合体
    不信可以注释掉，按如下步骤操作：
    1.重新cmake生成动态库
    2.在example里面重新make生成可执行文件testServer
    3.运行testServer之后，再在终端中运行命令 telnet localhost 8000即可复现这个问题

    回答：这是一个联合体，赋值顺序会影响到其他成员变量。主要使用的是ptr，fd完全没用到，因此建议删除fd的赋值代码
    */

    //event的使用地点见EPoller::fillActiveChannels()函数
    // event.data.fd = fd; //施磊老师此处对fd进行了赋值，但实际上这个赋值根本用不上，因此可以将其注释掉
    event.data.ptr = ch;

    // event.data.ptr = ch;
    // event.data.fd = fd;

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error : %d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add /mod error :%d\n", errno);
        }
    }
}