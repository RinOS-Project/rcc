struct Pair {
    int left;
    int right;
};

struct AnonymousView {
    union {
        struct {
            int left;
            int right;
        };
        long long packed;
    };
    int tail;
};

int copy_local(int left, int right) {
    struct Pair source = { left, right };
    struct Pair copy = source;
    return copy.left * 10 + copy.right;
}

int copy_pointer(struct Pair* source) {
    struct Pair copy = *source;
    return copy.left * 10 + copy.right;
}

int anonymous_members(int left, int right, int tail) {
    struct AnonymousView value = { 0 };
    value.left = left;
    value.right = right;
    value.tail = tail;
    return value.left * 100 + value.right * 10 + value.tail;
}
