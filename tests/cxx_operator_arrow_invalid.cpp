struct InvalidProxy {
    int operator->() {
        return 0;
    }
};

#if defined(TEST_INVALID_CLASS_ARROW_CHAIN)
struct EmptyArrowResult {};

struct ClassArrowProxy {
    EmptyArrowResult operator->() {
        return EmptyArrowResult{};
    }
};
#endif

int main() {
#if defined(TEST_INVALID_CLASS_ARROW_CHAIN)
    ClassArrowProxy value{};
    return value->missing;
#else
    InvalidProxy value{};
    return value->missing;
#endif
}
