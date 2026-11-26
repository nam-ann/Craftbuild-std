export module misc.number;

import std;

export {
	using i8 = std::int8_t;
	using i16 = std::int16_t;
	using i32 = std::int32_t;
	using i64 = std::int64_t;
	using u8 = std::uint8_t;
	using u16 = std::uint16_t;
	using u32 = std::uint32_t;
	using u64 = std::uint64_t;
	using f32 = float;
	using f64 = double;
	using usize = std::size_t;

	consteval auto operator"" b(unsigned long long v) noexcept -> i8 { return i8(v); }
	consteval auto operator"" w(unsigned long long v) noexcept -> i16 { return i16(v); }
	consteval auto operator"" dw(unsigned long long v) noexcept -> i32 { return i32(v); }
	consteval auto operator"" qw(unsigned long long v) noexcept -> i64 { return i64(v); }
	consteval auto operator"" ub(unsigned long long v) noexcept -> u8 { return u8(v); }
	consteval auto operator"" uw(unsigned long long v) noexcept -> u16 { return u16(v); }
	consteval auto operator"" udw(unsigned long long v) noexcept -> u32 { return u32(v); }
	consteval auto operator"" uqw(unsigned long long v) noexcept -> u64 { return u64(v); }
}