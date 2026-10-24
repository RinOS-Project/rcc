template <bool Condition, typename TrueType, typename FalseType>
struct conditional {
    using type = TrueType;
};

template <bool Condition, typename TrueType, typename FalseType>
using conditional_t = typename conditional<Condition, TrueType, FalseType>::type;

template <typename Type>
using identity_t = Type;

struct true_type {
    static constexpr bool value = true;
};

struct false_type {
    static constexpr bool value = false;
};

template <typename...>
struct direct_conjunction : true_type {};

template <typename First, typename... Rest>
struct direct_conjunction<First, Rest...>
    : direct_conjunction<Rest...> {};

template <typename...>
struct identity_conjunction : true_type {};

template <typename First, typename... Rest>
struct identity_conjunction<First, Rest...>
    : identity_t<identity_conjunction<Rest...>> {};

template <typename...>
struct conjunction : true_type {};

template <typename First>
struct conjunction<First> : First {};

template <typename First, typename... Rest>
struct conjunction<First, Rest...>
    : conditional_t<bool(First::value), conjunction<Rest...>, First> {};

static_assert(conjunction<true_type, true_type>::value,
              "two-element conjunction remains true");
static_assert(!conjunction<true_type, false_type>::value,
              "conjunction selects the first false operand");
static_assert(direct_conjunction<true_type, false_type>::value,
              "direct recursive pack expansion compiles");
static_assert(identity_conjunction<true_type, false_type>::value,
              "identity alias preserves recursive pack expansion");
