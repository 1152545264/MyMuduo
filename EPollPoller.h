#pragma once

#include "Poller.h"
#include "TimeStamp.h"

#include <vector>
#include <sys/epoll.h>

class Channel;

/***************
 * epoll的使用:
 * epoll_create
 * epoll_ctl  add /mod/del
 * epoll_wait
 * *************/

class EPoller : public Poller
{
public:
    EPoller(EventLoop *lopp);
    virtual ~EPoller() override;

    // 重写基类Poller的抽象方法
    virtual TimeStamp poll(int timeoutMs, ChannelLists *activeChannels) override;
    virtual void updateChannel(Channel *ch) override;
    virtual void removeChannel(Channel *ch) override;

private:
    // 填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelLists *activeChannels) const;
    // 更新channel通道
    void update(int operation, Channel *ch);

    // vector<epoll_event>的初始长度
    static const int kInitEventListSize = 16;

    using EventList = std::vector<epoll_event>;

    int epollfd_;      // 默认的是水平触发，未处理的事件会不断进行上报
    EventList events_; // 保存epollfd_上触发的事件集合，只会在poll函数中被我们所写的代码进行扩容，不会触发运行时自动动态扩容
};