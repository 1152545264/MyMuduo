#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

class InetAddress
{
public:
    explicit InetAddress(const sockaddr_in &addr) : addr_(addr) {}
    explicit InetAddress(uint16_t port=0, std::string ip = "127.0.0.1");
    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    const sockaddr_in *getSockAddr() const { return &addr_; }
    void setSockAddr(const sockaddr_in &addr) { addr_ = addr; }

private:
    sockaddr_in addr_;
};