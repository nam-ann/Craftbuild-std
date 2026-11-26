export module misc.raii;

import std;

using namespace std::literals;

template <typename T>
concept RAIICallable = requires (T t) { t(); };

export template <typename F>
requires RAIICallable<F>
class RAII {
    F val;
    
public:
    constexpr RAII() noexcept = delete;
    constexpr RAII(RAII const&) noexcept = delete;
    constexpr RAII(RAII&&) noexcept = delete;

    [[nodiscard]]
    constexpr RAII(F&& f) noexcept : val(std::forward<F>(f)) {}

    constexpr auto operator=(RAII const&) noexcept -> RAII& = delete;
    constexpr auto operator=(RAII&&) noexcept -> RAII& = delete;
    constexpr auto operator=(F&& f) noexcept -> RAII& {
        val();
        val = std::forward<F>(f);
        return *this;
    }

    constexpr auto override(F&& f) noexcept -> void { val = std::forward<F>(f); }
    
    ~RAII() noexcept { val(); }
};

export template <typename F>
RAII(F&&) -> RAII<std::decay_t<F>>;