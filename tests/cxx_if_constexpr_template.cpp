template<int N>
int select_positive(int value) {
    if constexpr (N > 0) {
        return value + N;
    } else {
        return no_such_symbol(value);
    }
}

template<int N>
int select_nonpositive(int value) {
    if constexpr (N <= 0) {
        return value - N;
    } else {
        return another_missing_symbol(value);
    }
}

template<typename T>
int select_sized(T value) {
    if constexpr (sizeof(T) == sizeof(int)) {
        return value;
    } else {
        return missing_size_branch(value);
    }
}

struct EnabledFlag {
    static constexpr int value = 1;
};

template<typename T>
int select_flag(int value) {
    if constexpr (T::value != 0) {
        return value + 2;
    } else {
        return missing_flag_branch(value);
    }
}

int main(void) {
    return select_positive<3>(4) == 7 &&
                   select_nonpositive<-2>(5) == 7 &&
                   select_sized(9) == 9 &&
                   select_flag<EnabledFlag>(5) == 7
               ? 0
               : 1;
}
