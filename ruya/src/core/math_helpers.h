#pragma once
#include <cstdint>

namespace ruya::math_helpers
{
	inline uint32_t AlignUp(uint32_t value, uint32_t alignment)
	{
		return (value + alignment - 1) & ~(alignment - 1);
	}
}