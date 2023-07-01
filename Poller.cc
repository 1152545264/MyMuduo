#include "Poller.h"
#include "Channel.h"

Poller::Poller(EventLoop *loop) : ownerLoop_(loop)
{
}

Poller::~Poller()
{
    
}

bool Poller::hasChannel(Channel *ch) const
{
    auto it = channelsMap_.find(ch->fd());
    return it != channelsMap_.end() && it->second == ch;
}