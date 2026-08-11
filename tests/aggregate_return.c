struct SmallResult {
    int first;
    int second;
    int third;
};

struct LargeResult {
    long long first;
    long long second;
    long long third;
};

static struct SmallResult make_small(int first, int second, int third) {
    struct SmallResult result = { first, second, third };
    return result;
}

static struct SmallResult forward_small(struct SmallResult value) {
    return value;
}

static int consume_small(struct SmallResult value) {
    return value.first * 100 + value.second * 10 + value.third;
}

static struct LargeResult make_large(int first, int second, int third) {
    struct LargeResult result = { first, second, third };
    return result;
}

int small_return_chain(int first, int second, int third) {
    return consume_small(forward_small(make_small(first, second, third)));
}

int small_return_member(int first, int second, int third) {
    return make_small(first, second, third).second;
}

int large_return_member(int first, int second, int third) {
    return (int)make_large(first, second, third).third;
}
