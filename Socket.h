#pragma once

#include "noncopyable.h"

class InetAddress;

// 封装socket fd
class Socket : noncopyable
{
public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}
    ~Socket();

    void bindAddress(const InetAddress &localAddr);
    void listen();
    int accept(InetAddress *perrAddr);
    void shutdownWrite();

    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);

    int fd() const { return sockfd_; }

private:
    const int sockfd_;
};