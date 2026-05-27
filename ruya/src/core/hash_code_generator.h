#pragma once
#include <cstdint>
#include <string>

#define XXH_INLINE_ALL
#include <xxhash/xxhash.h>

namespace ruya
{
    namespace hash_code_generator
    {
        inline uint64_t XXHash64(const std::string& string, uint64_t seed = 0)
        {
            uint64_t hash = XXH64(string.data(), string.size(), seed);

            if (hash == 0xffffffffffffffff)
            {
                hash = 0xfffffffffffffffe;
            }

            return hash;
        }
    }
}