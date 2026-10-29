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

template<class T0, class T1, class T2, class T3, class T4, class T5,
         class T6, class T7, class T8, class T9, class T10, class T11,
         class T12, class T13, class T14, class T15, class T16, class T17,
         class T18, class T19, class T20, class T21, class T22, class T23,
         class T24, class T25, class T26, class T27, class T28, class T29,
         class T30, class T31, class T32>
struct Aggregate33 {
    T0 field0; T1 field1; T2 field2; T3 field3; T4 field4; T5 field5;
    T6 field6; T7 field7; T8 field8; T9 field9; T10 field10; T11 field11;
    T12 field12; T13 field13; T14 field14; T15 field15; T16 field16;
    T17 field17; T18 field18; T19 field19; T20 field20; T21 field21;
    T22 field22; T23 field23; T24 field24; T25 field25; T26 field26;
    T27 field27; T28 field28; T29 field29; T30 field30; T31 field31;
    T32 field32;
};

template<class T0, class T1, class T2, class T3, class T4, class T5,
         class T6, class T7, class T8, class T9, class T10, class T11,
         class T12, class T13, class T14, class T15, class T16, class T17,
         class T18, class T19, class T20, class T21, class T22, class T23,
         class T24, class T25, class T26, class T27, class T28, class T29,
         class T30, class T31, class T32>
struct Constructed33 {
    int sum;
    Constructed33(
        T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10,
        T11, T12, T13, T14, T15, T16, T17, T18, T19, T20,
        T21, T22, T23, T24, T25, T26, T27, T28, T29, T30,
        T31, T32 value32)
        : sum(value32) {}
};

template<class T0, class T1, class T2, class T3, class T4, class T5,
         class T6, class T7, class T8, class T9, class T10, class T11,
         class T12, class T13, class T14, class T15, class T16, class T17,
         class T18, class T19, class T20, class T21, class T22, class T23,
         class T24, class T25, class T26, class T27, class T28, class T29,
         class T30, class T31, class T32>
struct Guided33 {
    int sum;
    Guided33(
        T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10,
        T11, T12, T13, T14, T15, T16, T17, T18, T19, T20,
        T21, T22, T23, T24, T25, T26, T27, T28, T29, T30,
        T31, T32 value32)
        : sum(value32) {}
};

template<class T0, class T1, class T2, class T3, class T4, class T5,
         class T6, class T7, class T8, class T9, class T10, class T11,
         class T12, class T13, class T14, class T15, class T16, class T17,
         class T18, class T19, class T20, class T21, class T22, class T23,
         class T24, class T25, class T26, class T27, class T28, class T29,
         class T30, class T31, class T32>
Guided33(T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11,
         T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23,
         T24, T25, T26, T27, T28, T29, T30, T31, T32)
    -> Guided33<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11,
                T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23,
                T24, T25, T26, T27, T28, T29, T30, T31, T32>;

int main() {
    Sum33Function function_pointer = sum_33_parameters;
    Aggregate33 aggregate{
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
        12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
        23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33};
    Constructed33 constructed(
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
        12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
        23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33);
    Guided33 guided(
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
        12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
        23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33);
    return function_pointer(
               1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
               12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
               23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33) == 561 &&
                   select_33_parameters(
                       1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
                       12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                       23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33) == 2 &&
                   aggregate.field0 + aggregate.field1 + aggregate.field2 +
                       aggregate.field3 + aggregate.field4 + aggregate.field5 +
                       aggregate.field6 + aggregate.field7 + aggregate.field8 +
                       aggregate.field9 + aggregate.field10 +
                       aggregate.field11 + aggregate.field12 +
                       aggregate.field13 + aggregate.field14 +
                       aggregate.field15 + aggregate.field16 +
                       aggregate.field17 + aggregate.field18 +
                       aggregate.field19 + aggregate.field20 +
                       aggregate.field21 + aggregate.field22 +
                       aggregate.field23 + aggregate.field24 +
                       aggregate.field25 + aggregate.field26 +
                       aggregate.field27 + aggregate.field28 +
                       aggregate.field29 + aggregate.field30 +
                       aggregate.field31 + aggregate.field32 == 561 &&
                   constructed.sum == 33 && guided.sum == 33
        ? 0
        : 1;
}
