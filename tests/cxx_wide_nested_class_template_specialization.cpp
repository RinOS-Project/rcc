struct WideNestedOwner {
    template <
        typename T0, typename T1, typename T2, typename T3,
        typename T4, typename T5, typename T6, typename T7,
        typename T8, typename T9, typename T10, typename T11,
        typename T12, typename T13, typename T14, typename T15,
        typename T16, typename T17, typename T18, typename T19,
        typename T20, typename T21, typename T22, typename T23,
        typename T24, typename T25, typename T26, typename T27,
        typename T28, typename T29, typename T30, typename T31,
        typename T32
    >
    struct Inner {
        char value;
    };
};

template<>
struct WideNestedOwner::Inner<
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int
> {
    int value;
};

using WideNestedInstance = WideNestedOwner::Inner<
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int
>;

int main() {
    return sizeof(WideNestedInstance) == sizeof(int) ? 0 : 1;
}
