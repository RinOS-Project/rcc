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

template<class T>
concept HasValueType = requires {
    typename T::value_type;
};

template<class T>
concept HasSize = requires(const T& value) {
    value.size();
};

template<class T>
concept HasValueTypeAndSize = HasValueType<T> && HasSize<T>;

template<HasValueType T>
int select_named_concept(T) {
    return 3;
}

template<HasValueTypeAndSize T>
int select_named_concept(T) {
    return 4;
}

struct SizedValueType {
    using value_type = int;
    int size() const { return 1; }
};

int main() {
    return select_constrained(3) == 2 &&
            select_named_concept(SizedValueType{}) == 4
        ? 0
        : 1;
}
