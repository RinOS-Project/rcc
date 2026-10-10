template<class T>
concept HasAdd = requires(T value) {
    value + 0;
};

template<class T>
concept Small = HasAdd<T> && (sizeof(T) <= 4 || sizeof(T) >= 8);

template<HasAdd T>
int select_constrained(T) {
    return 1;
}

template<Small T>
int select_constrained(T) {
    return 2;
}

int main() {
    return select_constrained(3) == 2 ? 0 : 1;
}
