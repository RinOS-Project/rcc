template <typename Left, typename Right,
          bool BothLvalue = false, bool BothRvalue = false,
          bool RvalueLvalue = false, bool LvalueRvalue = false>
struct reference_selection {};

template <typename Left, typename Right>
struct reference_selection<Left, Right, false, false, false, true>
    : reference_selection<Right, Left> {};

template <int Depth>
struct priority_chain : priority_chain<Depth - 1> {};

template <>
struct priority_chain<0> {};

template <typename ValueType>
struct injected_current_type {
    using type = injected_current_type;
};

template <typename Left, typename Right>
struct same_type {
    static constexpr bool value = false;
};

template <typename Type>
struct same_type<Type, Type> {
    static constexpr bool value = true;
};

static_assert(sizeof(priority_chain<3>) == 1,
              "recursive partial-specialization base resolves defaults");
static_assert(same_type<injected_current_type<int>::type,
                        injected_current_type<int>>::value,
              "an injected class name denotes the current specialization");
