#pragma once
#include <core/assert.h>

#define CHECK_VKRESULT(exp)                                                                               \
    do                                                                                                    \
    {                                                                                                     \
        VkResult result = (exp);                                                                          \
        ENGINE_FATAL_ASSERT(result == VK_SUCCESS,                                                         \
                            "[VULKAN ERROR] {}: Failed with VkResult: {}",                                \
                            #exp, static_cast<int>(result));                                              \
    } while (0)

#define CHECK_VKRESULT_DEBUG(exp)                                                                         \
    do                                                                                                    \
    {                                                                                                     \
        VkResult result = (exp);                                                                          \
        (void)result;                                                                                     \
        ENGINE_ASSERT_MSG(result == VK_SUCCESS,                                                           \
                          "[VULKAN ERROR] {}: Failed with VkResult: {}",                                  \
                          #exp, static_cast<int>(result));                                                \
    } while (0)