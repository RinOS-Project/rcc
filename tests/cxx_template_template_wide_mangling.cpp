template<
    class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11,
    class P12, class P13, class P14, class P15, class P16, class P17, class P18, class P19, class P20, class P21, class P22,
    class P23, class P24, class P25, class P26, class P27, class P28, class P29, class P30, class P31, class P32, class P33
>
struct WideTemplate {};

template<
    template<
        class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11,
        class P12, class P13, class P14, class P15, class P16, class P17, class P18, class P19, class P20, class P21, class P22,
        class P23, class P24, class P25, class P26, class P27, class P28, class P29, class P30, class P31, class P32, class P33
    > class Template,
    class T>
int wide_template_argument(
    Template<
        T, T, T, T, T, T, T, T, T, T, T,
        T, T, T, T, T, T, T, T, T, T, T,
        T, T, T, T, T, T, T, T, T, T, T
    >* value) {
    return value != nullptr;
}

using WideArgument = WideTemplate<
    int, int, int, int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int, int, int, int
>;

int run_wide_template_argument() {
    WideArgument value;
    return wide_template_argument(&value);
}

template<
    int N1, int N2, int N3, int N4, int N5, int N6, int N7, int N8, int N9, int N10, int N11,
    int N12, int N13, int N14, int N15, int N16, int N17, int N18, int N19, int N20, int N21, int N22,
    int N23, int N24, int N25, int N26, int N27, int N28, int N29, int N30, int N31, int N32, int N33
>
struct WideValues {};

template<int Value>
int wide_nontype_template_argument(
    WideValues<
        Value, Value, Value, Value, Value, Value, Value, Value, Value, Value, Value,
        Value, Value, Value, Value, Value, Value, Value, Value, Value, Value, Value,
        Value, Value, Value, Value, Value, Value, Value, Value, Value, Value, Value
    >* value) {
    return value != nullptr;
}

using WideValuesArgument = WideValues<
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
>;

int run_wide_nontype_template_argument() {
    WideValuesArgument value;
    return wide_nontype_template_argument(&value);
}

template<
    class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11,
    class P12, class P13, class P14, class P15, class P16, class P17, class P18, class P19, class P20, class P21, class P22,
    class P23, class P24, class P25, class P26, class P27, class P28, class P29, class P30, class P31, class P32, class P33
>
struct WidePartialInput {};

template<class T>
struct WidePartialMatch {
    static constexpr int value = 0;
};

template<class T>
struct WidePartialMatch<WidePartialInput<
        T, T, T, T, T, T, T, T, T, T, T,
        T, T, T, T, T, T, T, T, T, T, T,
        T, T, T, T, T, T, T, T, T, T, T
>> {
    static constexpr int value = 1;
};

using WidePartialArgument = WidePartialInput<
    int, int, int, int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int, int, int, int
>;
static_assert(WidePartialMatch<WidePartialArgument>::value == 1);

int main() {
    return run_wide_template_argument() &&
                   run_wide_nontype_template_argument()
        ? 0
        : 1;
}
