template<class T>
concept HasAdd = requires(T value) {
    value + 0;
};

template<class T>
concept Small = HasAdd<T> && sizeof(T) <= 4;

template<Small T>
int constrained_value(T value) {
    return value + 20;
}

int main() {
    return constrained_value(3LL);
}
