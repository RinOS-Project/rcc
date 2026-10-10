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

int main() {
    int value = 1;
    int values[2] = {};
    short priority = 0;
    return select_crossed(&value, priority, 0) +
           select_qualified_crossed(&value, priority) +
           select_array_qualified_crossed(values, priority);
}
