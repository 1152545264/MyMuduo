#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "TimeStamp.h"

#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;

/**************************
 * TcpServer ==> Acceptor ==> 新用户连接，通过accept函数拿到connfd
 *
 * ==> TcpConnection设置回调 ==> Channel注册到Poller上 ===> Poller监听到事件发生则调用相应的回调
 **************************/

// Connection的一部分数据+Socket打包成Channel交给Poller
// Poller监听到Channel上有事件发生时调用相应的回调函数
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop, const std::string &name, int sockfd,
                  const InetAddress &localAddr, const InetAddress &peerAddr);
    ~TcpConnection();

    // 发送数据
    void send(const std::string &buf);
    // 关闭连接
    void shutdown();
    // 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();

    EventLoop *getLoop() const { return loop_; }
    const std::string name() { return name_; }
    const InetAddress &localAddress() { return localAddr_; }
    const InetAddress &perrAddress() { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    void setConnectionCallback(const ConnectionCallback &cb)
    {
        connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback &cb)
    {
        messageCallback_ = cb;
    }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb, size_t highWaterMark)
    {
        writeCompleteCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }
    void setCloseCallback(const CloseCallback &cb)
    {
        closeCallback_ = cb;
    }

private:
    enum StateE
    {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    };

    void setState(StateE state) { state_ = state; }

    void handleRead(TimeStamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void *data, size_t len);

    void shutdownInLoop();

    EventLoop *loop_; // 此处不死baseLoop，因为TcpConnection都是在Subloop上管理的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    // 此处和Acceptor相似 Acceptor在mainLoop中，TcpConnection在subLoop中
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    const InetAddress localAddr_; // 当前主机的IP+port
    const InetAddress peerAddr_;  // 对端主机的IP+port

    ConnectionCallback connectionCallback_;       // 有新连接时的回调
    MessageCallback messageCallback_;             // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调
    CloseCallback closeCallback_; //由TcpServer对其进行赋值
    HighWaterMarkCallback highWaterMarkCallback_;
    size_t highWaterMark_; // 高水位线（多少算水位），避免接收方接受速率太慢发送速率太快

    // 应用生产数据的速度可能会快于网络层和数据链路层的发送速度，\
    因此加入了缓冲区
    Buffer inputBuffer_; // 接收数据的缓冲区
    Buffer ouputBuffer_; // 发送数据的缓冲区
};