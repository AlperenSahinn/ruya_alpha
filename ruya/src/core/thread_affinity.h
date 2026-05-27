#pragma once
#include <cstdint>

#define NOMINMAX
#include <Windows.h>

namespace ruya
{
    inline void PinThreadToCore(uint32_t coreIndex)
    {
        if (coreIndex >= 64) return;
        SetThreadAffinityMask(GetCurrentThread(), 1ULL << coreIndex);
    }
}