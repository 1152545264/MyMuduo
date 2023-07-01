#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>

static int createNonBlocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0) 
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reusePort)
    : loop_(loop), 
	acceptSocket_(createNonBlocking()), 
	acceptChannel_(loop, acceptSocket_.fd()),
      listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr); // bind函数
    // TCPServer::start() 调用Acceptor.listen() :  新用户的连接需要执行一个回调(confd ==> channel ==> subLoop)
    // baseLoop ==> acceptChannel_(listenfd_) ==>
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen(); // listen
    acceptChannel_.enableReading();
}

// listenfd有事件发生了，即有新用户进行连接了，
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0)
    {
        if (newConnectionCb_) //newConnectionCb_由acceptor所在的TcpServer调用setNewConnectionCallback()函数进行设置
        {
            //newConnectionCb_被注册为TcpServer::newConnection
            newConnectionCb_(connfd, peerAddr); // 轮询找到subLoop，唤醒分发当前新客户端的Channel
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept err: %d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE) // 资源用完了
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}