template<typename T>
T identity(T value) {
    T copy{};
    copy = value;
    return copy;
}

template<typename T>
T* identity_ptr(T* value) {
    return value;
}

namespace detail {
template<typename T>
T passthrough(T value) {
    return value;
}

template<typename T, typename U = int>
U default_result(T value) {
    U result{};
    result = value;
    return result;
}
}

int call_identity(int value) {
    return identity<int>(value);
}

long call_identity_long(long value) {
    return identity<long>(value);
}

int* call_identity_ptr(int* value) {
    return identity_ptr<int>(value);
}

int call_namespaced_template(int value) {
    return detail::passthrough<int>(value);
}

int call_default_template(int value) {
    return detail::default_result<int>(value);
}
