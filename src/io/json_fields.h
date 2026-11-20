#pragma once
#include "core/vec3.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>

namespace json_fields {
using Json = nlohmann::json;

inline void expect_fields(const Json& object, std::initializer_list<std::string_view> names) {
    for (const auto& field : object.get_ref<const Json::object_t&>()) {
        if (std::find(names.begin(), names.end(), field.first) == names.end())
            throw std::runtime_error("Unknown field: " + field.first);
    }
    // Required fields are read with at(), which reports the missing key.
}

inline double number(const Json& value, const std::string& field) {
    if (!value.is_number()) throw std::runtime_error(field + " must be a number");
    return value.get<double>();
}

inline double optional_number(const Json& object, const std::string& field, double fallback) {
    const auto it = object.find(field);
    return it == object.end() ? fallback : number(*it, field);
}

inline Vec3 vector3(const Json& object, const std::string& field) {
    const Json& v = object.at(field);
    if (!v.is_array() || v.size() != 3)
        throw std::runtime_error(field + " must be an array of three numbers");
    return {number(v[0], field), number(v[1], field), number(v[2], field)};
}

inline Vec3 rgb(const Json& object, const std::string& field) {
    const Vec3 value = vector3(object, field);
    if (std::min({value.x, value.y, value.z}) < 0)
        throw std::runtime_error(field + " must have nonnegative channels");
    return value;
}
}  // namespace json_fields
