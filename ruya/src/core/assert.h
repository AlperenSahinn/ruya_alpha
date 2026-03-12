#pragma once
#include <cstdlib>
#include <string>
#include "log.h"

#if !defined(NDEBUG)
#define ENGINE_DEBUG 1
#else
#define ENGINE_DEBUG 0
#endif

#if ENGINE_DEBUG
#if defined(_MSC_VER)
#define ENGINE_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
#define ENGINE_DEBUG_BREAK() __builtin_trap()
#else
#define ENGINE_DEBUG_BREAK() std::abort()
#endif
#else
#define ENGINE_DEBUG_BREAK() ((void)0)
#endif

#if defined(_MSC_VER)
#define ENGINE_FUNCTION __FUNCSIG__
#else
#define ENGINE_FUNCTION __PRETTY_FUNCTION__
#endif

inline void EngineAssertHandler(
    const char* expression,
    const char* message,
    const char* file,
    int line,
    const char* function)
{
    RUYA_LOG_CRITICAL(
        "\n=== ENGINE ASSERT FAILED ===\n"
        "Expression: {}\n"
        "Message   : {}\n"
        "File      : {}\n"
        "Line      : {}\n"
        "Function  : {}\n"
        "============================",
        expression,
        message ? message : "None",
        file,
        line,
        function
    );
}

inline void EngineAssertHandler(
    const char* expression,
    const std::string& message,
    const char* file,
    int line,
    const char* function)
{
    EngineAssertHandler(expression, message.c_str(), file, line, function);
}

#if ENGINE_DEBUG
#define ENGINE_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            EngineAssertHandler( \
                #expr, \
                nullptr, \
                __FILE__, \
                __LINE__, \
                ENGINE_FUNCTION \
            ); \
            ENGINE_DEBUG_BREAK(); \
        } \
    } while (0)

#define ENGINE_ASSERT_MSG(expr, ...) \
    do { \
        if (!(expr)) { \
            EngineAssertHandler( \
                #expr, \
                fmt::format(__VA_ARGS__), \
                __FILE__, \
                __LINE__, \
                ENGINE_FUNCTION \
            ); \
            ENGINE_DEBUG_BREAK(); \
        } \
    } while (0)
#else
#define ENGINE_ASSERT(expr) ((void)0)
#define ENGINE_ASSERT_MSG(expr, ...) ((void)0)
#endif

#define ENGINE_FATAL_ASSERT(expr, ...) \
    do { \
        if (!(expr)) { \
            EngineAssertHandler( \
                #expr, \
                fmt::format(__VA_ARGS__), \
                __FILE__, \
                __LINE__, \
                ENGINE_FUNCTION \
            ); \
            ENGINE_DEBUG_BREAK(); \
            std::abort(); \
        } \
    } while (0)

#define ENGINE_STATIC_ASSERT(...) \
    static_assert(__VA_ARGS__)

#define ENGINE_STATIC_ASSERT_MSG(...) \
    static_assert(__VA_ARGS__)