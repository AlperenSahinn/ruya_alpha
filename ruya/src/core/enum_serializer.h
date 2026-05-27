#pragma once
#include <string>

#include <stdexcept>
#include <nlohmann_json/json.hpp>
#include <magic_enum/magic_enum.hpp>

#include "assert.h"

#define MAKE_ENUM_JSON_SERIALIZABLE(EnumType) \
    inline void to_json(nlohmann::json& j, const EnumType& e) { \
        j = magic_enum::enum_name(e); \
    } \
    inline void from_json(const nlohmann::json& j, EnumType& e) { \
        auto parsed = magic_enum::enum_cast<EnumType>(j.get<std::string>()); \
        if (parsed.has_value()) e = parsed.value(); \
        else throw std::invalid_argument(std::string(#EnumType) + " invalid value: " + j.get<std::string>()); \
    }