template<int First, int Second>
struct NonTypeOrder {
    int value;
};

template<int Repeated>
struct NonTypeOrder<Repeated, Repeated> {
    int value;
};

template<int FixedPrefix>
struct NonTypeOrder<FixedPrefix, 5> {
    int value;
};

NonTypeOrder<5, 5> ambiguous_non_type_order;
