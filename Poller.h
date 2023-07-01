#pragma once
#include "noncopyable.h"
#include "TimeStamp.h"

#include <vector>
#include <unordered_map>

class Channel;
class EventLoop;

// muduo库中多路事件分发器的IO复用模块
class Poller : public noncopyable
{
public:
    using ChannelLists = std::vector<Channel *>;

    Poller(EventLoop *loop);
    virtual ~Poller();

    // 给所有IO复用保留同样的接口
    virtual TimeStamp poll(int timeoutMs, ChannelLists *activeChannels) = 0;
    virtual void updateChannel(Channel *ch) = 0;
    virtual void removeChannel(Channel *ch) = 0;

    // 判断参数Channel是否在当前Poller中
    bool hasChannel(Channel *ch) const;

    // EventLoop 可以通过该接口获取默认的IO复用的具体实现
    static Poller *newDefaultPoller(EventLoop *loop);

protected:
    //map的key表示sockfd, value表示sockfd所属的channel通道
    using ChannelMap = std::unordered_map<int, Channel *>;
    ChannelMap channelsMap_;

private:
    EventLoop *ownerLoop_;
};