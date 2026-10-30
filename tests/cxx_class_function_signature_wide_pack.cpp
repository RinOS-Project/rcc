template<class Signature>
struct WideFunctionSignature {
    static constexpr int value = -1;
};

template<
    class R,
    class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7,
    class P8, class P9, class P10, class P11, class P12, class P13, class P14, class P15,
    class P16, class P17, class P18, class P19, class P20, class P21, class P22, class P23,
    class P24, class P25, class P26, class P27, class P28, class P29, class P30, class P31,
    class... Tail
>
struct WideFunctionSignature<
    R(
        P0, P1, P2, P3, P4, P5, P6, P7,
        P8, P9, P10, P11, P12, P13, P14, P15,
        P16, P17, P18, P19, P20, P21, P22, P23,
        P24, P25, P26, P27, P28, P29, P30, P31,
        Tail...
    )
> {
    static constexpr int value = 22;
};

using ActualWideSignature = void(
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    char,
    short);

int main() {
    return WideFunctionSignature<ActualWideSignature>::value == 22
        ? 0
        : 1;
}
