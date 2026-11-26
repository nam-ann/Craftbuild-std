
export module misc.number;

export {
	using i8 = signed char;
	using i16 = signed short;
	using i32 = signed int;
	using i64 = signed long long;
	using u8 = unsigned char;
	using u16 = unsigned short;
	using u32 = unsigned int;
	using u64 = unsigned long long;
	using f32 = float;
	using f64 = double;
	using usize = size_t;

	consteval auto operator"" b(unsigned long long v) noexcept -> i8 { return i8(v); }
	consteval auto operator"" w(unsigned long long v) noexcept -> i16 { return i16(v); }
	consteval auto operator"" dw(unsigned long long v) noexcept -> i32 { return i32(v); }
	consteval auto operator"" qw(unsigned long long v) noexcept -> i64 { return i64(v); }
	consteval auto operator"" ub(unsigned long long v) noexcept -> u8 { return u8(v); }
	consteval auto operator"" uw(unsigned long long v) noexcept -> u16 { return u16(v); }
	consteval auto operator"" udw(unsigned long long v) noexcept -> u32 { return u32(v); }
	consteval auto operator"" uqw(unsigned long long v) noexcept -> u64 { return u64(v); }
}