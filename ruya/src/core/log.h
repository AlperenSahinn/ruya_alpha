#pragma once

#if !defined(NDEBUG)
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#else
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_WARN
#endif

#include <spdlog/spdlog.h>

#define RUYA_LOG_DEBUG(...)    SPDLOG_DEBUG(__VA_ARGS__)
#define RUYA_LOG_INFO(...)     SPDLOG_INFO(__VA_ARGS__)
#define RUYA_LOG_WARN(...)     SPDLOG_WARN(__VA_ARGS__)
#define RUYA_LOG_ERROR(...)    SPDLOG_ERROR(__VA_ARGS__)
#define RUYA_LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

namespace ruya
{
    inline void InitLogger()
    {
#if !defined(NDEBUG)
        spdlog::set_level(spdlog::level::debug);
        RUYA_LOG_INFO("Logger initilized. Log level: Debug");
#else
        spdlog::set_level(spdlog::level::warn);
        RUYA_LOG_INFO("Logger initilized. Log level: Warning");
#endif
    }
}
