#include <iostream>
#include "Logger.h"
#include "TimeStamp.h"

Logger &Logger::instance() // 获取唯一的实例对象
{
    static Logger logger;
    return logger;
}
void Logger::setLogLevel(int level) // 设置日志级别
{
    logLevel_ = level;
}

// 写日志 [级别信息] time : msg
void Logger::log(std::string msg)
{
    switch (logLevel_)
    {
    case INFO:
        std::cout << "[INFO] ";
        break;
    case DEBUG:
        std::cout << "[DEBUG] ";
        break;
    case ERROR:
        std::cout << "[ERROR] ";
        break;
    case FATAL:
        std::cout << "[FATAL] ";
        break;

    default:
        break;
    }

    // 打印时间和msg
    std::cout << TimeStamp::now().toString() << " : " << msg << std::endl;
}