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
T* pointer_identity(T* value) {
    return value;
}

template<typename T, typename U = long>
U default_deduced(T value) {
    U result{};
    result = value;
    return result;
}

int call_deduced_template() {
    return identity(3);
}

long call_deduced_long_template() {
    return identity(7L);
}

int* call_deduced_pointer_template(int* value) {
    return pointer_identity(value);
}

long call_deduced_default_template() {
    return default_deduced(5);
}

template<typename T>
T passthrough(T value) {
    return value;
}
}
