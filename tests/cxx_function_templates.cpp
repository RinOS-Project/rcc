template<typename T>
[[nodiscard]] constexpr T identity(T value) noexcept {
    T copy{};
    copy = value;
    return copy;
}

template<typename T, int Count = 1>
T* address_of(T& value) noexcept {
    return &value;
}

template<typename T>
class holder final {
public:
    T value;
    holder& assign(holder&& other) noexcept {
        if (this != &other) value = other.value;
        return *this;
    }
};

namespace detail {
[[nodiscard]] inline int doubled(int value) noexcept {
    return value + value;
}

template<typename T>
T passthrough(T value) {
    return value;
}
}
