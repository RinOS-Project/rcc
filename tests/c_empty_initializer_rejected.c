struct Pair {
    int first;
    int second;
};

struct Pair invalid_empty_initializer(void) {
    return (struct Pair){};
}
