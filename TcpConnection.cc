#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <strings.h>
#include <error.h>
#include <string>
#include <functional>

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, const std::string &nameArg, int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop)),
      name_(nameArg),
      state_(kConnecting),
      reading_(true),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64 * 1024 * 1024) // 64M

{
    /***
     * 下面给channel设置相应的回到函数，Poller给channel通知感兴趣的事件发生了，channel会
     * 回调相应的回调函数（对应于Channel::handleEvent方法）
     * **/
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead,
                                        this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d \n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n",
             name_.c_str(), channel_->fd(), (int)state_);
}



/************
 * 发送数据 应用写得快 而内核发送数据慢，需要把数据写入应用层缓冲区，
 * 而且设置了水位回调
 * **********/
void TcpConnection::send(const std::string &buf)
{
    if (state_ == kConnected)
    {
        // 判断当前线程是否在loop所处的线程中
        if (loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
        }
    }
}

void TcpConnection::sendInLoop(const void *data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remainning = len;
    bool faluError = false;
    // 之前调用过该Connection的shutdown 不能再进行发送了
    if (state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing");
        return;
    }
    // 表示Channel_第一次开始写数据，而且缓冲区没有数据
    if (!channel_->isWriting() && ouputBuffer_.writeAbleBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0)
        {
            remainning = len - nwrote;
            if (remainning == 0 && writeCompleteCallback_)
            {
                // 既然在此处数据全部发送完成，就不用再给Channel设置epollout事件了
                loop_->queueInLoop(std::bind(
                    writeCompleteCallback_, shared_from_this()));
            }
        }
        else // nwrote < 0
        {

            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {
                LOG_ERROR("Tcp::sendInLoop");
            }
            // SIGPIPE RESET
            if (errno == EPIPE || errno == ECONNRESET)
            {
                faluError = true;
            }
        }
    }
    /*
    说明当前这一次write，并没有把数据全部发送出去，剩余的数据需要保存到缓冲区当中，
    然后给Channel注册epollout事件(即对应的fd可写事件)，poller发现tcp的发送缓冲区有空间，
    会通知相应的 socke_channel，调用writeCallback回调函数(即TcpConnection::handleWrite())
    也就是调用TcpConnection::handleWrite方法，把发送缓冲区中的数据全部发送完成
    */
    if (!faluError && remainning > 0)
    {
        // 目前发送缓冲区剩余的待发送数据的长度
        size_t oldLen = ouputBuffer_.readAbleBytes();
        if (oldLen + remainning >= highWaterMark_ // TODO 如何理解这两行代码（42集29:44）
            && oldLen < highWaterMark_            //
            && highWaterMarkCallback_)
        {
            // 调用水位线回调函数
            loop_->queueInLoop(std::bind(highWaterMarkCallback_,
                                         shared_from_this(),
                                         oldLen + remainning));
        }
        ouputBuffer_.append((char *)data + nwrote, remainning);
        if (!channel_->isWriting())
        {
            // 这里一定要注册channel的写事件，否则poller不会给channel通知epollout事件
            channel_->enableWriting();
        }
    }
}

// 关闭连接，供用户程序员使用
void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting()) // 说明ouputBuffer中的数据已经全部发送完成
    {
        /*
        关闭写端，Poller就会给channel通知关闭事件，
        从而回调TcpConnection::handleClose
        */
        socket_->shutdownWrite();
    }
}

// 连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading(); // 向Poller注册channel的epollin事件

    // 新连接建立，执行回调
    connectionCallback_(shared_from_this());
}

// 连接销毁
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll(); // 把channel的所有感兴趣事件从poller中删除掉
        connectionCallback_(shared_from_this());
    }
    channel_->remove(); // 把channel从poller中删除掉
}
void TcpConnection::handleRead(TimeStamp receiveTime)
{
    int saveErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);
    if (n > 0) // 有数据
    {
        // 已建立连接的用户，有可读事件发生了，调用用户层传入的回调操作OnMessage(\
        在TcpConnection::setMessageCallback()函数中进行设置的)
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0) // 客户端断开连接
    {
        handleClose();
    }
    else
    {
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if (channel_->isWriting())
    {
        int saveErrno = 0;
        ssize_t n = ouputBuffer_.writeFd(channel_->fd(), &saveErrno);
        if (n > 0)
        {
            ouputBuffer_.retrieve(n);              // n个字节的数据已经处理过了
            if (ouputBuffer_.readAbleBytes() == 0) // 发送完成
            {
                channel_->disableWriting();
                if (writeCompleteCallback_)
                {
                    // 唤醒loop对应的thread线程，执行回调
                    loop_->queueInLoop(std::bind(
                        writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else
    {
        LOG_ERROR("Connection fd=%d is down, no more writing \n", channel_->fd());
    }
}

// poller => channel::closeCallback() =>TcpConnection::handleClose()
void TcpConnection::handleClose()
{
    LOG_INFO("fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();
    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr); // 执行连接关闭的回调
    closeCallback_(connPtr);      // 关闭连接的回调 执行的是TcpServer::removeConnection回调方法
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof(optval);
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET,
                     SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s- SOL_ERROR:%d \n",
              name_.c_str(), err);
}