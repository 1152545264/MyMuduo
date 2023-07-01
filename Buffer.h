#pragma once

#include <cstddef> //size_t类型的定义文件
#include <vector>
#include <string>
#include <algorithm>

// 网络库底层的缓冲器类型定义
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;   // 数据包大小，用于解决粘包问题
    static const size_t kInitialSize = 1024; // 数据缓冲区初始化大小

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize),
          readerIndex_(kCheapPrepend),
          writerIndex_(kCheapPrepend)
    {
    }

    size_t readAbleBytes() const
    {
        return writerIndex_ - readerIndex_;
    }

    size_t writeAbleBytes() const
    {
        return buffer_.size() - writerIndex_;
    }

    size_t prependAbleBytes() const
    {
        return readerIndex_;
    }

    // 返回缓冲区中可读数据的起始地址
    const char *peek() const
    {
        return begin() + readerIndex_;
    }

    // OnMessage string <- Buffer
    void retrieve(size_t len)
    {
        if (len < readAbleBytes())
        {
            // 应用只读取了可读缓冲区中长度为len的数据，
            // 未读数据起始区间为：[reader_index_+len, writerIndex_]
            readerIndex_ += len;
        }
        else // len == readbAbleBytes()
        {
            retrieveAll();
        }
    }

    // 把OnMessage函数上报的Buffer数据转成string类型的数据返回
    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    std::string retrieveAllAsString()
    {
        return retrieveAsString(readAbleBytes()); // 应用可读取的数据长度
    }

    std::string retrieveAsString(size_t len)
    {
        // 把缓冲区中可读取的数据读取出来
        std::string result(peek(), len);

        // 对缓冲区进行复位操作
        retrieve(len);

        return result;
    }

    void ensureWriteAbleBytes(size_t len)
    {
        if (writeAbleBytes() < len)
        {
            makeSpace(len); // 扩容函数
        }
    }

    // 把[data,data+len]内存上的数据添加到writeable缓冲区当中
    void append(const char *data, size_t len)
    {
        ensureWriteAbleBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    char *beginWrite()
    {
        return begin() + writerIndex_;
    }
    const char *beginWrite() const
    {
        return begin() + writerIndex_;
    }

    // 从fd文件描述符上读取数据
    ssize_t readFd(int fd, int *saveErrno);
    // 通过fd发送数据
    ssize_t writeFd(int fd, int *saveErrno);

private:
    // 获取vector底层首元素的地址，也就是数组的起始地址获取
    char *begin()
    {
        return &*buffer_.begin();
    }

    const char *begin() const
    {
        return &*buffer_.begin();
    }

    // 缓冲区扩容操作
    void makeSpace(size_t len)
    {
        /*
        kCheapPrepend | reader | writer
        kCheapPrepend | len         |
        */
        if (writeAbleBytes() + prependAbleBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            size_t readable = readAbleBytes();
            std::copy(begin() + readerIndex_,
                      begin() + writerIndex_,
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};
