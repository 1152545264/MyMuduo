#pragma once

#include <string>

#include "noncopyable.h"

// 定义日志的级别INFO  ERROR FATAL DEBUG WARNING

enum LogLevel
{
    INFO,  // 普通信息
    ERROR, // 错误信息
    FATAL, // core信息
    DEBUG, // 调试信息
};

// 输出一个日志类
class Logger : noncopyable
{
public:
    static Logger &instance();   // 获取唯一的实例对象
    void setLogLevel(int level); // 设置日志级别
    void log(std::string msg);   // 写日志

private:
    // 设置日志级别
    int logLevel_;
    Logger() {}
};

#define LOG_INFO(logmsgformat, ...)                       \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::instance();              \
        logger.setLogLevel(INFO);                         \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgformat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0);

#ifdef MUDEBUG
#define LOG_DEBUG(logmsgformat, ...)                      \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::instance();              \
        logger.setLogLevel(DEBUG);                        \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgformat, ##__VA_ARGS__); \
        logger.log(buf);                                   \
    } while (0);
#else
#define LOG_DEBUG(logmsgformat, ...)
#endif

#define LOG_ERROR(logmsgformat, ...)                      \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::instance();              \
        logger.setLogLevel(ERROR);                        \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgformat, ##__VA_ARGS__); \
        logger.log(buf);                                   \
    } while (0);

#define LOG_FATAL(logmsgformat, ...)                      \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::instance();              \
        logger.setLogLevel(FATAL);                        \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgformat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
        exit(-1);                                         \
    } while (0);
