#pragma once

#include <iostream>

// 时间类
class TimeStamp
{
public:
    TimeStamp();
    explicit TimeStamp(int64_t microSecondsSinceEpoch); // 防止隐式转换
    static TimeStamp now();
    std::string toString() const;

private:
    int64_t microSecondsSinceEpoch_;
};