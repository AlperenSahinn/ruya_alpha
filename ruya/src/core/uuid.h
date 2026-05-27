#pragma once
#include <cstdint>
#include <random>
#include <string>
#include <functional>
#include <cstdio>
#include <chrono>
#include <ostream>

#include <nlohmann_json/json.hpp>
#include <spdlog/fmt/fmt.h>

namespace ruya
{
    struct UUID
    {
        uint64_t high = 0;
        uint64_t low = 0;

        bool operator==(const UUID& other) const {
            return high == other.high && low == other.low;
        }

        bool operator!=(const UUID& other) const {
            return !(*this == other);
        }

        bool operator<(const UUID& other) const {
            return high != other.high ? (high < other.high) : (low < other.low);
        }

        bool IsValid() const { return high != 0 && low != 0; }

        static UUID Invalid() { return UUID{ 0, 0 }; }

        std::string ToString() const
        {
            char buf[37];
            std::snprintf(buf, sizeof(buf),
                "%08x-%04x-%04x-%04x-%012",
                static_cast<uint32_t>(high >> 32),
                static_cast<uint32_t>((high >> 16) & 0xFFFF),
                static_cast<uint32_t>(high & 0xFFFF),
                static_cast<uint32_t>(low >> 48),
                static_cast<unsigned long long>(low & 0xFFFFFFFFFFFFULL)
            );
            return { buf };
        }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(UUID, high, low)
    };

    inline std::ostream& operator<<(std::ostream& os, const UUID& uuid)
    {
        return os << uuid.ToString();
    }

    inline UUID GenerateRandomUUID()
    {
        thread_local std::mt19937_64 gen(
            std::random_device{}() ^
            static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())
        );

        std::uniform_int_distribution<uint64_t> dis;

        UUID uuid;
        uuid.high = dis(gen);
        uuid.low = dis(gen);

        uuid.high = (uuid.high & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
        uuid.low = (uuid.low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

        return uuid;
    }
}

template<>
struct std::hash<ruya::UUID>
{
    size_t operator()(const ruya::UUID& uuid) const noexcept
    {
        size_t h = uuid.high ^ (uuid.high >> 33);
        h *= 0xFF51AFD7ED558CCDULL;
        h ^= uuid.low ^ (uuid.low >> 33);
        h *= 0xC4CEB9FE1A85EC53ULL;
        return h;
    }
};

namespace ruya::guid {
    inline std::string to_string(const UUID& uuid) {
        return uuid.ToString();
    }
}

template <>
struct fmt::formatter<ruya::UUID> {
    constexpr auto parse(format_parse_context& ctx) const {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const ruya::UUID& uuid, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", uuid.ToString());
    }
};