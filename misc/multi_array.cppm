export module misc.multi_array;

import std;
import misc.number;

template <typename T, usize... S>
struct multi_array_t {
    using type = T;
};

template <typename T, usize First, usize... Rest>
struct multi_array_t<T, First, Rest...> {
    using type = std::array<typename multi_array_t<T, Rest...>::type, First>;
};

export template <typename T, usize... S>
using multi_array = typename multi_array_t<T, S...>::type;