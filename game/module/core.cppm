module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/wrapped.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/math_defs.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector3i.hpp>
ENABLE_WARNING

export module game.core;

import std;

import misc.string;
import misc.number;

using namespace godot;

export namespace craftbuild {
    using fsize = godot::real_t;

    consteval auto operator"" fz(long double value) { return fsize(value); }

    inline constexpr auto VERSION = "26.8"fs;
    inline constexpr auto FULL_VERSION = "indev " + VERSION;

    inline auto craftbuild_debug = true;
    inline auto log_verbose = true;
    inline auto colored_log = true;

    inline auto render_distance = 32dw;
    inline auto cpu_sleep_time = 180ub;

    inline constexpr auto MATH_PI = 3.1415926535897932384626433833fz;
    inline constexpr auto MATH_TAU = 6.2831853071795864769252867666fz;
    inline constexpr auto MAXIMUM_CAMERA_ANGLE = 2.0000002384185791015625fz;

    auto to_gstring(std::string_view s) -> String { return String::utf8(s.data(), s.size()); }
}

template <typename T>
struct std::hash<Ref<T>> final {
    usize operator()(Ref<T> const& value) const { return usize(*value); }
};

template <typename CharT>
struct std::formatter<CharStringT<CharT>, CharT> : formatter<basic_string_view<CharT>, CharT> {
    auto format(CharStringT<CharT> const& str, auto& ctx) const {
        auto const sv = basic_string_view<CharT>(str.get_data(), str.length());
        return formatter<basic_string_view<CharT>, CharT>::format(sv, ctx);
    }
};

template <>
struct std::formatter<Vector3> : formatter<string_view> {
    auto format(Vector3 const& v, auto& ctx) const {
        return formatter<string_view>::format("({} {} {})"f(v.x, v.y, v.z), ctx);
    }
};

template <>
struct std::formatter<Vector3i> : formatter<string_view> {
    auto format(Vector3i const& v, auto& ctx) const {
        return formatter<string_view>::format("({} {} {})"f(v.x, v.y, v.z), ctx);
    }
};