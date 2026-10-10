template<typename T>
int select_crossed(const void*, int, T) {
    return 1;
}

template<typename T>
int select_crossed(T, long, int) {
    return 2;
}

template<typename T>
int select_qualified_crossed(const T*, int) {
    return 3;
}

template<typename T>
int select_qualified_crossed(T, long) {
    return 4;
}

template<typename T>
int select_array_qualified_crossed(const T*, int) {
    return 5;
}

template<typename T>
int select_array_qualified_crossed(T*, long) {
    return 6;
}

template<typename T>
int select_incomparable_qualification(const T*) {
    return 7;
}

template<typename T>
int select_incomparable_qualification(volatile T*) {
    return 8;
}

template<typename T>
int select_incomparable_void_cv(T*, const void*) {
    return 9;
}

template<typename T>
int select_incomparable_void_cv(T*, volatile void*) {
    return 10;
}

struct UnrelatedBaseLeft {};
struct UnrelatedBaseRight {};
struct UnrelatedDerived : UnrelatedBaseLeft, UnrelatedBaseRight {};

struct UnrelatedValueBaseLeft {
    int value;
};

struct UnrelatedValueBaseRight {
    int value;
};

struct UnrelatedValueDerived : UnrelatedValueBaseLeft,
                               UnrelatedValueBaseRight {};

struct InaccessibleValueBase {
    int value;
};

struct InaccessibleValueDerived : private InaccessibleValueBase {};

template<typename T>
int select_unrelated_pointer_bases(T*, UnrelatedBaseLeft*) {
    return 11;
}

template<typename T>
int select_unrelated_pointer_bases(T*, UnrelatedBaseRight*) {
    return 12;
}

template<typename T>
int select_unrelated_value_bases(T*, UnrelatedValueBaseLeft) {
    return 13;
}

template<typename T>
int select_unrelated_value_bases(T*, UnrelatedValueBaseRight) {
    return 14;
}

template<typename T>
int select_inaccessible_base_value(T*, InaccessibleValueBase) {
    return 15;
}

int main() {
    int value = 1;
    int values[2] = {};
    UnrelatedDerived unrelated;
    UnrelatedValueDerived unrelated_value;
    InaccessibleValueDerived inaccessible_value;
    short priority = 0;
    return select_crossed(&value, priority, 0) +
           select_qualified_crossed(&value, priority) +
           select_array_qualified_crossed(values, priority) +
           select_incomparable_qualification(&value) +
           select_incomparable_void_cv(&value, &value) +
           select_unrelated_pointer_bases(&unrelated, &unrelated) +
           select_unrelated_value_bases(&unrelated_value, unrelated_value) +
           select_inaccessible_base_value(
               &inaccessible_value, inaccessible_value);
}
