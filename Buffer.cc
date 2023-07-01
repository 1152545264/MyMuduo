#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/*
从fd文件描述符上读取数据 Poller工作在LT模式上
Buffer缓冲区是有大小的，但是从fd上读取数据的时候，
却不知道tcp数据的最终大小
*/
ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    char extraBuf[65536] = {0}; // 栈上的内存空间, 64k
    struct iovec vec[2];

    // 这是buffer底层缓冲区剩余的可写空间大小
    const size_t writeable = writeAbleBytes();
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writeable;

    vec[1].iov_base = extraBuf;
    vec[1].iov_len = sizeof(extraBuf);

    const int iovcnt = (writeable < sizeof(extraBuf)) ? 2 : 1;
    // readv系统调用可以向多个缓冲区写入数据
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writeable) // Buffer的可读写缓冲区已经够存储读出来的数据了
    {
        writerIndex_ += n;
    }
    else // extraBuf里面也写入了数据
    {
        writerIndex_ = buffer_.size();
        append(extraBuf, n - writeable); // writerIndex开始写n - writeable大小的数据
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int *saveErrno)
{
    ssize_t n = ::write(fd, peek(), readAbleBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}