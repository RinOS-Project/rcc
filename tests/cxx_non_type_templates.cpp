template<int N>
int add_constant(int value) {
    return value + N;
}

template<int N = 4>
int add_default_constant(int value) {
    return value + N;
}

template<typename T, int N>
T add_typed_constant(T value) {
    return value + N;
}

template<int N, int M = N + 1>
int add_default_from_value(int value) {
    return value + M;
}

int call_add_three(int value) {
    return add_constant<3>(value);
}

int call_add_negative(int value) {
    return add_constant<-2>(value);
}

int call_add_default(int value) {
    return add_default_constant<>(value);
}

long call_typed_constant(long value) {
    return add_typed_constant<long, 7>(value);
}

int call_default_from_value(int value) {
    return add_default_from_value<3>(value);
}
