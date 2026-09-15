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

struct OddCopy {
    int value;
    unsigned char tail;
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

int assign_chain(int left, int right) {
    struct Pair source = { left, right };
    struct Pair middle = { 0 };
    struct Pair target = { 0 };
    target = middle = source;
    return target.left * 1000 + target.right * 100 +
           middle.left * 10 + middle.right;
}

int assign_odd(int value, int tail) {
    struct OddCopy source = { value, (unsigned char)tail };
    struct OddCopy target = { 0 };
    target = source;
    return target.value * 1000 + target.tail;
}

int anonymous_members(int left, int right, int tail) {
    struct AnonymousView value = { 0 };
    value.left = left;
    value.right = right;
    value.tail = tail;
    return value.left * 100 + value.right * 10 + value.tail;
}

int sum_pair(struct Pair value) {
    return value.left * 10 + value.right;
}

int conditional_argument(int condition) {
    return sum_pair(condition ? (struct Pair){ 3, 8 }
                             : (struct Pair){ 4, 7 });
}

int comma_argument(void) {
    struct Pair source = { 1, 2 };
    return sum_pair((source, (struct Pair){ 5, 6 }));
}
