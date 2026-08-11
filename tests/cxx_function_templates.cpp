template<typename T>
[[nodiscard]] constexpr T identity(T value) noexcept {
    return value;
}

template<typename T, int Count = 1>
T* address_of(T& value) noexcept {
    return &value;
}

template<typename T>
class holder final {
public:
    T value;
};

namespace detail {
template<typename T>
T passthrough(T value) {
    return value;
}
}
