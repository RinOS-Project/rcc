template<int N> concept Positive = N > 0;

template<int N> requires Positive<N>
int positive_value() {
    return N;
}

template<int N> requires (!Positive<N>)
int nonpositive_value() {
    return N;
}

template<int N>
constexpr bool positive_concept_id() {
    return Positive<N>;
}

struct ConceptCarrier {
    int value;
};

template<typename T>
concept HasValue = requires(T candidate) {
    candidate.value;
};

template<typename T>
constexpr bool has_value_concept_id() {
    return HasValue<T>;
}

template<typename T>
concept SmallReturnType = sizeof(T) <= sizeof(int);

template<typename T, typename Bound>
concept ReturnTypeAtMost = sizeof(T) <= sizeof(Bound);

template<typename T, typename... Expected>
concept ReturnTypeSizeMatches = ((sizeof(T) == sizeof(Expected)) && ...);

template<typename T, int... ExpectedSizes>
concept ReturnTypeMatchesAnySize = ((sizeof(T) == ExpectedSizes) || ...);

template<typename T>
struct SameSizeTemplate {
    T value;
};

template<typename T>
struct LargerTemplate {
    char prefix;
    T value;
};

template<typename T, template<typename> class... Templates>
concept ReturnTemplatePackMatches =
    ((sizeof(Templates<T>) == sizeof(T)) && ...);

template<typename T, typename A0, typename A1, typename A2, typename A3,
         typename A4, typename A5, typename A6, typename A7, typename A8,
         typename A9, typename A10, typename A11, typename A12, typename A13,
         typename A14, typename A15, typename A16, typename A17, typename A18,
         typename A19, typename A20, typename A21, typename A22, typename A23,
         typename A24, typename A25, typename A26, typename A27, typename A28,
         typename A29, typename A30, typename A31>
concept ManyReturnTypeArguments = sizeof(T) > 0;

template<typename T, int ExpectedSize>
concept ReturnTypeSize = sizeof(T) == ExpectedSize;

struct SmallSizeResult {
    int value;
};

struct LargeSizeResult {
    char value[8];
};

struct SmallSizeProvider {
    SmallSizeResult size() const { return {}; }
};

struct LargeSizeProvider {
    LargeSizeResult size() const { return {}; }
};

template<typename T>
concept HasSmallSizeResult = requires(T candidate) {
    { candidate.size() } -> SmallReturnType;
};

template<typename T>
concept HasSizeResultWithinInt = requires(T candidate) {
    { candidate.size() } -> ReturnTypeAtMost<int>;
};

template<typename T>
concept HasSizeMatchingInt = requires(T candidate) {
    { candidate.size() } -> ReturnTypeSizeMatches<int>;
};

template<typename T>
concept HasSizeMatchingEmptyPack = requires(T candidate) {
    { candidate.size() } -> ReturnTypeSizeMatches;
};

template<typename T>
concept HasSizeMatchingAnyExpectedSize = requires(T candidate) {
    { candidate.size() } -> ReturnTypeMatchesAnySize<4>;
};

template<typename T>
concept HasSameSizeTemplateReturn = requires(T candidate) {
    { candidate.size() } -> ReturnTemplatePackMatches<SameSizeTemplate>;
};

template<typename T>
concept HasLargerTemplateReturn = requires(T candidate) {
    { candidate.size() } -> ReturnTemplatePackMatches<LargerTemplate>;
};

template<typename T>
concept HasManyReturnTypeArguments = requires(T candidate) {
    { candidate.size() } -> ManyReturnTypeArguments<
        int, int, int, int, int, int, int, int,
        int, int, int, int, int, int, int, int,
        int, int, int, int, int, int, int, int,
        int, int, int, int, int, int, int, int>;
};

template<typename T>
concept HasFourByteSizeResult = requires(T candidate) {
    { candidate.size() } -> ReturnTypeSize<4>;
};

template<typename T, int ExpectedSize>
concept HasSizeResultOf = requires(T candidate) {
    { candidate.size() } -> ReturnTypeSize<ExpectedSize>;
};

template<typename T>
requires HasSmallSizeResult<T>
int small_return_type_accepts(T) {
    return 1;
}

template<typename T>
requires (!HasSmallSizeResult<T>)
int small_return_type_accepts(T) {
    return 0;
}

template<typename T>
requires HasSizeResultWithinInt<T>
int explicit_return_type_accepts(T) {
    return 1;
}

template<typename T>
requires (!HasSizeResultWithinInt<T>)
int explicit_return_type_accepts(T) {
    return 0;
}

template<typename T>
requires HasFourByteSizeResult<T>
int non_type_return_constraint_accepts(T) {
    return 1;
}

template<typename T>
requires (!HasFourByteSizeResult<T>)
int non_type_return_constraint_accepts(T) {
    return 0;
}

template<typename T>
requires HasSizeResultOf<T, 4>
int dependent_non_type_return_constraint_accepts(T) {
    return 1;
}

template<typename T>
requires (!HasSizeResultOf<T, 4>)
int dependent_non_type_return_constraint_accepts(T) {
    return 0;
}

template<typename T>
requires HasValue<T>
int read_value(T candidate) {
    return candidate.value;
}

template<HasValue T>
int read_value_constrained(T candidate) {
    return candidate.value + 1;
}

struct NoValue {
};

template<typename T>
requires (!HasValue<T>)
int read_missing(T) {
    return 7;
}

template<typename T>
requires ReturnTypeSizeMatches<T, int>
int named_type_pack_constraint(T) {
    return 1;
}

template<typename T>
requires (!ReturnTypeSizeMatches<T, int>)
int named_type_pack_constraint(T) {
    return 0;
}

template<typename T>
requires ReturnTypeMatchesAnySize<T, 4>
int named_non_type_pack_constraint(T) {
    return 1;
}

template<typename T>
requires (!ReturnTypeMatchesAnySize<T, 4>)
int named_non_type_pack_constraint(T) {
    return 0;
}

int main() {
    ConceptCarrier carrier = {41};
    NoValue missing = {};
    return Positive<3> && !Positive<-2> &&
                   positive_concept_id<3>() &&
                   !positive_concept_id<-2>() &&
                   HasValue<ConceptCarrier> && !HasValue<NoValue> &&
                   has_value_concept_id<ConceptCarrier>() &&
                   !has_value_concept_id<NoValue>() &&
                   positive_value<3>() == 3 &&
                   nonpositive_value<-2>() == -2 &&
                   read_value(carrier) == 41 &&
                   read_value_constrained(carrier) == 42 &&
                   read_missing(missing) == 7 &&
                   small_return_type_accepts(SmallSizeProvider{}) == 1 &&
                   small_return_type_accepts(LargeSizeProvider{}) == 0 &&
                   explicit_return_type_accepts(SmallSizeProvider{}) == 1 &&
                   explicit_return_type_accepts(LargeSizeProvider{}) == 0 &&
                   HasSizeMatchingInt<SmallSizeProvider> &&
                   !HasSizeMatchingInt<LargeSizeProvider> &&
                   HasSizeMatchingEmptyPack<SmallSizeProvider> &&
                   HasSizeMatchingAnyExpectedSize<SmallSizeProvider> &&
                   !HasSizeMatchingAnyExpectedSize<LargeSizeProvider> &&
                   HasSameSizeTemplateReturn<SmallSizeProvider> &&
                   !HasLargerTemplateReturn<SmallSizeProvider> &&
                   HasManyReturnTypeArguments<SmallSizeProvider> &&
                   named_type_pack_constraint(SmallSizeProvider{}) == 1 &&
                   named_type_pack_constraint(LargeSizeProvider{}) == 0 &&
                   named_non_type_pack_constraint(SmallSizeProvider{}) == 1 &&
                   named_non_type_pack_constraint(LargeSizeProvider{}) == 0 &&
                   non_type_return_constraint_accepts(
                       SmallSizeProvider{}) == 1 &&
                   non_type_return_constraint_accepts(
                       LargeSizeProvider{}) == 0 &&
                   dependent_non_type_return_constraint_accepts(
                       SmallSizeProvider{}) == 1 &&
                   dependent_non_type_return_constraint_accepts(
                       LargeSizeProvider{}) == 0
               ? 0
               : 1;
}
