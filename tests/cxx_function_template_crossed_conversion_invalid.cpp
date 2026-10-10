template<typename T>
int select_crossed(const void*, int, T) {
    return 1;
}

template<typename T>
int select_crossed(T, long, int) {
    return 2;
}

int main() {
    int value = 1;
    short priority = 0;
    return select_crossed(&value, priority, 0);
}
