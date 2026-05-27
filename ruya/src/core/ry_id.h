#pragma once
#include <functional>
#include <string>
#include <ostream>

#include <nlohmann_json/json.hpp>
#include <spdlog/fmt/fmt.h>

#include "assert.h"

namespace ruya
{
    class RyID
    {
    private:
        static constexpr uint64_t cInvalidRyID = 0xffffffffffffffff;

    public:
        explicit RyID(uint64_t inID)
            : id(inID)
        {
            ENGINE_ASSERT(inID != cInvalidRyID);
        }

        RyID()
            : id(cInvalidRyID)
        {
        }

        ~RyID() = default;

        inline uint64_t GetRawID() const
        {
            return id;
        }

        inline bool IsValid() const
        {
            return id != cInvalidRyID;
        }

        static RyID Invalid() { return RyID{}; }

        inline bool operator==(RyID ryID) const
        {
            return id == ryID.id;
        }

        inline bool operator!=(RyID ryID) const
        {
            return id != ryID.id;
        }

        inline bool operator<(RyID ryID) const
        {
            return id < ryID.id;
        }

        friend void to_json(nlohmann::json& j, const RyID& ryID) 
        {
            j = nlohmann::json{ {"RyID", ryID.id} };
        }

        friend void from_json(const nlohmann::json& j, RyID& ryID) 
        {
            j.at("RyID").get_to(ryID.id);
        }

    private:
        uint64_t id;
    };

    inline std::ostream& operator<<(std::ostream& os, RyID ryID)
    {
        return os << ryID.GetRawID();
    }
}

namespace std
{
    template<>
    struct hash<ruya::RyID>
    {
        inline size_t operator()(ruya::RyID ryID) const
        {
            return std::hash<uint64_t>{}(ryID.GetRawID());
        }
    };

    inline std::string to_string(ruya::RyID ryID)
    {
        return std::to_string(ryID.GetRawID());
    }
}

template <>
struct fmt::formatter<ruya::RyID> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const ruya::RyID& id, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", std::to_string(id));
    }
};