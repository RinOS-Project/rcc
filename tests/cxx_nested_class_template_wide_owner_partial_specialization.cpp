template<
    class O1, class O2, class O3, class O4, class O5, class O6, class O7, class O8, class O9, class O10, class O11,
    class O12, class O13, class O14, class O15, class O16, class O17, class O18, class O19, class O20, class O21, class O22,
    class O23, class O24, class O25, class O26, class O27, class O28, class O29, class O30, class O31, class O32, class OwnerValue
>
struct WideNestedOwner {
    template<class Value>
    struct Inner {
        int result() const { return 1; }
    };
};

template<
    class O1, class O2, class O3, class O4, class O5, class O6, class O7, class O8, class O9, class O10, class O11,
    class O12, class O13, class O14, class O15, class O16, class O17, class O18, class O19, class O20, class O21, class O22,
    class O23, class O24, class O25, class O26, class O27, class O28, class O29, class O30, class O31, class O32, class OwnerValue
>
struct WideNestedOwner<
    O1, O2, O3, O4, O5, O6, O7, O8, O9, O10, O11,
    O12, O13, O14, O15, O16, O17, O18, O19, O20, O21, O22,
    O23, O24, O25, O26, O27, O28, O29, O30, O31, O32, OwnerValue*
> {
    template<class Value>
    struct Inner {
        int result() const { return 2; }
    };
};

template<
    class O1, class O2, class O3, class O4, class O5, class O6, class O7, class O8, class O9, class O10, class O11,
    class O12, class O13, class O14, class O15, class O16, class O17, class O18, class O19, class O20, class O21, class O22,
    class O23, class O24, class O25, class O26, class O27, class O28, class O29, class O30, class O31, class O32, class OwnerValue
>
template<class Value>
struct WideNestedOwner<
    O1, O2, O3, O4, O5, O6, O7, O8, O9, O10, O11,
    O12, O13, O14, O15, O16, O17, O18, O19, O20, O21, O22,
    O23, O24, O25, O26, O27, O28, O29, O30, O31, O32, OwnerValue*
>::Inner<Value*> {
    int result() const { return 3; }
};

int main() {
    WideNestedOwner<
    int, int, int, int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int, int, int, char*
    >::Inner<int*> value;
    return value.result() == 3 ? 0 : 1;
}
