#pragma once

#if !defined(NDEBUG)
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#else
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#define RUYA_LOG_DEBUG(...)    SPDLOG_DEBUG(__VA_ARGS__)
#define RUYA_LOG_INFO(...)     SPDLOG_INFO(__VA_ARGS__)
#define RUYA_LOG_WARN(...)     SPDLOG_WARN(__VA_ARGS__)
#define RUYA_LOG_ERROR(...)    SPDLOG_ERROR(__VA_ARGS__)
#define RUYA_LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

namespace ruya
{
    namespace detail
    {
        inline std::string MakeTimestampedLogPath()
        {
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            std::tm tm_buf{};
#ifdef _WIN32
            localtime_s(&tm_buf, &time_t_now);
#else
            localtime_r(&time_t_now, &tm_buf);
#endif
            std::ostringstream oss;
            oss << "logs/ruya_"
                << std::put_time(&tm_buf, "%Y-%m-%d_%H-%M-%S")
                << ".log";
            return oss.str();
        }

        inline void PruneOldLogs(std::size_t keep_count = 10)
        {
            try
            {
                std::vector<std::filesystem::directory_entry> entries;
                for (const auto& entry : std::filesystem::directory_iterator("logs"))
                {
                    if (entry.is_regular_file() &&
                        entry.path().extension() == ".log")
                    {
                        entries.push_back(entry);
                    }
                }

                if (entries.size() <= keep_count) return;

                std::sort(entries.begin(), entries.end(),
                    [](const auto& a, const auto& b) {
                        return a.last_write_time() > b.last_write_time();
                    });

                for (std::size_t i = keep_count; i < entries.size(); ++i)
                {
                    std::error_code ec;
                    std::filesystem::remove(entries[i].path(), ec);
                }
            }
            catch (...) { }
        }

        inline void SignalHandler(int sig)
        {
            const char* name = "unknown";
            switch (sig)
            {
            case SIGSEGV: name = "SIGSEGV"; break;
            case SIGABRT: name = "SIGABRT"; break;
            case SIGFPE:  name = "SIGFPE";  break;
            case SIGILL:  name = "SIGILL";  break;
            case SIGTERM: name = "SIGTERM"; break;
            }
            if (auto logger = spdlog::default_logger())
            {
                logger->critical("Crash detected: signal {}", name);
                logger->flush();
            }
            spdlog::shutdown();
            std::_Exit(EXIT_FAILURE);
        }

#ifdef _WIN32
        inline LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* info)
        {
            if (auto logger = spdlog::default_logger())
            {
                logger->critical("Unhandled SEH exception: code 0x{:X}",
                    info->ExceptionRecord->ExceptionCode);
                logger->flush();
            }
            spdlog::shutdown();
            return EXCEPTION_EXECUTE_HANDLER;
        }
#endif

        inline void TerminateHandler()
        {
            if (auto logger = spdlog::default_logger())
            {
                logger->critical("std::terminate called (uncaught exception)");
                logger->flush();
            }
            spdlog::shutdown();
            std::_Exit(EXIT_FAILURE);
        }

        inline void InstallCrashHandlers()
        {
            std::signal(SIGSEGV, SignalHandler);
            std::signal(SIGABRT, SignalHandler);
            std::signal(SIGFPE, SignalHandler);
            std::signal(SIGILL, SignalHandler);
            std::signal(SIGTERM, SignalHandler);
#ifdef _WIN32
            SetUnhandledExceptionFilter(UnhandledExceptionHandler);
#endif
            std::set_terminate(TerminateHandler);
        }
    }

    inline void InitLogger()
    {
        try
        {
            std::filesystem::create_directories("logs");

            detail::PruneOldLogs(10);

            const std::string log_path = detail::MakeTimestampedLogPath();

            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern("[%H:%M:%S.%e] [%^%l%$] [%s:%#] %v");

            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                log_path, false);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] [thread %t] %v");

#if !defined(NDEBUG)
            const auto level = spdlog::level::debug;
#else
            const auto level = spdlog::level::info;
#endif
            console_sink->set_level(level);
            file_sink->set_level(level);

            std::vector<spdlog::sink_ptr> sinks{ console_sink, file_sink };
            auto logger = std::make_shared<spdlog::logger>(
                "ruya", sinks.begin(), sinks.end());

            logger->set_level(level);
            logger->flush_on(spdlog::level::warn);

            spdlog::set_default_logger(logger);
            spdlog::flush_every(std::chrono::seconds(3));

            detail::InstallCrashHandlers();

            RUYA_LOG_INFO("Logger initialized. Output: {}", log_path);
        }
        catch (const spdlog::spdlog_ex& ex)
        {
            std::fprintf(stderr, "Log init failed: %s\n", ex.what());
        }
    }

    inline void ShutdownLogger()
    {
        spdlog::shutdown();
    }
}