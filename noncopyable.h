#pragma once

// 继承自noncopyable的子类可以被正常的构造和析构，但是无法进行拷贝构造和赋值
class noncopyable
{
public:
    noncopyable(const noncopyable &) = delete;
    noncopyable &operator=(const noncopyable &) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;
};