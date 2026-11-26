export module misc.string;

import std;
import misc.number;

using namespace std::literals;

export template <usize N>
struct fixed_string final {
    std::array<char, N> buf;

    consteval fixed_string() = default;

    template <usize... I>
    consteval fixed_string(char const(&s)[N + 1], std::index_sequence<I...>) : buf{ s[I]... } {}
    consteval fixed_string(char const(&s)[N + 1]) : fixed_string(s, std::make_index_sequence<N>()) {}

    consteval fixed_string(std::array<char, N> const& arr) : buf(arr) {}

    template <usize N2>
    consteval auto operator+(fixed_string<N2> const& other) const {
        constexpr auto new_n = N + N2;
        auto new_buf = std::array<char, new_n>{};

        for (auto i : std::views::iota(0uz, N))  new_buf[i] = buf[i];
        for (auto i : std::views::iota(0uz, N2)) new_buf[N + i] = other.buf[i];

        return fixed_string<new_n>(new_buf);
    }

    template <usize N2>
    consteval auto operator+(char const(&s)[N2]) const {
        return *this + fixed_string<N2 - 1>(s);
    }

    template <usize N2>
    constexpr auto operator==(fixed_string<N2> const& other) const {
        if constexpr (N != N2) return false;
        else {
            for (auto [e1, e2] : std::views::zip(buf, other.buf)) {
                if (e1 != e2) return false;
            }
            return true;
        }
    }

    constexpr auto operator[](usize i) const noexcept { return buf[i]; }

    constexpr operator std::string_view() const noexcept { return { buf.data(), N }; }

    constexpr auto data() const noexcept { return buf.data(); }
    constexpr auto size() const noexcept { return N; }

    constexpr auto begin() const noexcept { return buf.begin(); }
    constexpr auto end() const noexcept { return buf.end(); }

    template <usize N2>
    friend consteval auto operator+(char const(&s)[N2], fixed_string const& self) {
        return fixed_string<N2 - 1>(s) + self;
    }
};

template <usize N>
struct std::formatter<fixed_string<N>> final : formatter<string_view> {
    auto format(fixed_string<N> const& s, auto& ctx) const {
        return formatter<string_view>::format(s, ctx);
    }
};

export template <usize N>
fixed_string(char const(&)[N]) -> fixed_string<N - 1>;

export template <fixed_string S>
constexpr auto operator"" fs() { return S; }

export template <fixed_string S>
constexpr auto operator"" f() { return [](auto&&... args) { return std::format(std::string_view(S), std::forward<decltype(args)>(args)...); }; }

export struct string_hash final {
    using is_transparent = void;

    auto operator()(std::string_view sv) const noexcept -> usize {
        return std::hash<std::string_view>()(sv);
    }
};