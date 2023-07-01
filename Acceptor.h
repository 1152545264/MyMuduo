#pragma once

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

#include <functional>

class EventLoop;
class InetAddress;

class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reusePort);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb) //由Acceptor所在的TcpServer进行设置
    {
        newConnectionCb_ = cb;
    }

    bool listenning() const { return listenning_; }
    void listen();
private:
    void handleRead();

    EventLoop *loop_; // Acceptor用的就死用户定义的那个baseLoop，也称作mainLoop
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCb_;
    bool listenning_;
};