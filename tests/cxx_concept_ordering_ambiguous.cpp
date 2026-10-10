template<class T>
concept HasAdd = requires(T value) {
    value + 0;
};

template<class T>
concept HasSubtract = requires(T value) {
    value - 0;
};

template<class T>
concept Small = HasAdd<T> && (sizeof(T) <= 4 || sizeof(T) >= 8);

template<HasAdd T>
int select_ambiguous(T) {
    return 1;
}

template<HasSubtract T>
int select_ambiguous(T) {
    return 2;
}

template<Small T>
int select_ambiguous(T) {
    return 3;
}

int main() {
    return select_ambiguous(3);
}
