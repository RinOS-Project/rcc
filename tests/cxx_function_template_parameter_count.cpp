template<class T0, class T1, class T2, class T3, class T4, class T5,
         class T6, class T7, class T8, class T9, class T10, class T11,
         class T12, class T13, class T14, class T15, class T16, class T17,
         class T18, class T19, class T20, class T21, class T22, class T23,
         class T24, class T25, class T26, class T27, class T28, class T29,
         class T30, class T31, class T32>
int sum_33_parameters(
    T0 value0, T1 value1, T2 value2, T3 value3, T4 value4, T5 value5,
    T6 value6, T7 value7, T8 value8, T9 value9, T10 value10, T11 value11,
    T12 value12, T13 value13, T14 value14, T15 value15, T16 value16,
    T17 value17, T18 value18, T19 value19, T20 value20, T21 value21,
    T22 value22, T23 value23, T24 value24, T25 value25, T26 value26,
    T27 value27, T28 value28, T29 value29, T30 value30, T31 value31,
    T32 value32) {
    return value0 + value1 + value2 + value3 + value4 + value5 + value6 +
           value7 + value8 + value9 + value10 + value11 + value12 + value13 +
           value14 + value15 + value16 + value17 + value18 + value19 +
           value20 + value21 + value22 + value23 + value24 + value25 +
           value26 + value27 + value28 + value29 + value30 + value31 + value32;
}

template<class T0, class T1, class T2, class T3, class T4, class T5,
         class T6, class T7, class T8, class T9, class T10, class T11,
         class T12, class T13, class T14, class T15, class T16, class T17,
         class T18, class T19, class T20, class T21, class T22, class T23,
         class T24, class T25, class T26, class T27, class T28, class T29,
         class T30, class T31, class T32>
int select_33_parameters(
    T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11,
    T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23,
    T24, T25, T26, T27, T28, T29, T30, T31, T32) {
    return 1;
}

template<class T0, class T1, class T2, class T3, class T4, class T5,
         class T6, class T7, class T8, class T9, class T10, class T11,
         class T12, class T13, class T14, class T15, class T16, class T17,
         class T18, class T19, class T20, class T21, class T22, class T23,
         class T24, class T25, class T26, class T27, class T28, class T29,
         class T30, class T31>
int select_33_parameters(
    T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11,
    T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23,
    T24, T25, T26, T27, T28, T29, T30, T31, int) {
    return 2;
}

using Sum33Function = int (*)(
    int, int, int, int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int, int, int, int, int);

int main() {
    Sum33Function function_pointer = sum_33_parameters;
    return function_pointer(
               1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
               12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
               23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33) == 561 &&
                   select_33_parameters(
                       1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
                       12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                       23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33) == 2
        ? 0
        : 1;
}
