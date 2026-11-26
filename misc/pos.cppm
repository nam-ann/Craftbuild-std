module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
ENABLE_WARNING

export module misc.pos;

import std;

import misc.string;
import misc.number;

using namespace godot;

template<typename T1, typename T2>
concept AbleToCast = requires (T2 t2) { T1(t2); };

export template <typename T>
requires std::is_arithmetic_v<T>
struct Pos3D final {
    T x, y, z;

    constexpr Pos3D() = default;
    constexpr Pos3D(T x, T y, T z) : x(x), y(y), z(z) {}
    constexpr Pos3D(Vector3 const& v) : x(v.x), y(v.y), z(v.z) {}
    constexpr Pos3D(Vector3i const& v) : x(v.x), y(v.y), z(v.z) {}
    template<typename T2>
    requires AbleToCast<T, T2>
    constexpr Pos3D(Pos3D<T2> const& pos) : x((T)pos.x), y((T)pos.y), z((T)pos.z) {}

#define def_operator(op) constexpr Pos3D& operator op##=(Pos3D const& other) { x op##= other.x, y op##= other.y, z op##= other.z; return *this;}

    def_operator(+);
    def_operator(-);
    def_operator(*);
    def_operator(/);
    def_operator(%);

#undef def_operator
#define def_operator(op) constexpr Pos3D operator op(Pos3D const& other) const { Pos3D result; result.x op##= other.x, result.y op##= other.y, result.z op##= other.z; return result;}

    def_operator(+);
    def_operator(-);
    def_operator(*);
    def_operator(/);
    def_operator(%);

#undef def_operator

    constexpr operator godot::Vector3() const {
        return godot::Vector3(f32(x), f32(y), f32(z));
    }
    constexpr operator godot::Vector3i() const {
        return godot::Vector3i(i32(x), i32(y), i32(z));
    }

    constexpr auto operator==(Pos3D const& other) const -> bool {
        return x == other.x and y == other.y and z == other.z;
    }
};

export template <typename T>
requires std::is_arithmetic_v<T>
struct Pos2D final {
    T x, y;

    constexpr Pos2D() = default;
    constexpr Pos2D(T x, T y) : x(x), y(y) {}
    constexpr Pos2D(Vector2 const& v) : x(v.x), y(v.y) {}
    constexpr Pos2D(Vector2i const& v) : x(v.x), y(v.y) {}
    template<typename T2>
    requires AbleToCast<T, T2>
    constexpr Pos2D(Pos2D<T2> const& pos) : x((T)pos.x), y((T)pos.y) {}

#define def_operator(op) constexpr Pos2D& operator op##=(Pos2D const& other) { x op##= other.x, y op##= other.y; return *this;}

    def_operator(+);
    def_operator(-);
    def_operator(*);
    def_operator(/);
    def_operator(%);

#undef def_operator
#define def_operator(op) constexpr Pos2D operator op(Pos2D const& other) const { Pos2D result; result.x op##= other.x, result.y op##= other.y; return result;}

    def_operator(+);
    def_operator(-);
    def_operator(*);
    def_operator(/);
    def_operator(%);

#undef def_operator

    constexpr operator godot::Vector2() const {
        return godot::Vector2(f32(x), f32(y));
    }
    constexpr operator godot::Vector2i() const {
        return godot::Vector2i(i32(x), i32(y));
    }

    constexpr auto operator==(Pos2D const& other) const -> bool {
        return x == other.x and y == other.y;
    }
};

template <typename T>
requires std::is_arithmetic_v<T>
struct std::hash<Pos3D<T>> final {
    auto operator()(Pos3D<T> const& pos) const noexcept -> usize {
        return hash<T>()(pos.x) ^ (hash<T>()(pos.y) << 16) ^ (hash<T>()(pos.z) << 8);
    }
};

template <typename T>
requires std::is_arithmetic_v<T>
struct std::formatter<Pos3D<T>> final : formatter<string> {
    auto format(Pos3D<T> const& pos, auto& ctx) const {
        return formatter<string>::format("({}, {}, {})"f(pos.x, pos.y, pos.z), ctx);
    }
};

template <typename T>
requires std::is_arithmetic_v<T>
struct std::hash<Pos2D<T>> final {
    auto operator()(Pos2D<T> const& pos) const noexcept -> usize {
        return hash<T>()(pos.x) ^ (hash<T>()(pos.y) << 16);
    }
};

template <typename T>
requires std::is_arithmetic_v<T>
struct std::formatter<Pos2D<T>> final : formatter<string> {
    auto format(Pos2D<T> const& pos, auto& ctx) const {
        return formatter<string>::format("({}, {})"f(pos.x, pos.y), ctx);
    }
};