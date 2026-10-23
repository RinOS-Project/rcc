template<int N> concept Positive = N > 0;

template<int N> requires Positive<N>
int positive_value() {
    return N;
}

template<int N> requires (!Positive<N>)
int nonpositive_value() {
    return N;
}

struct ConceptCarrier {
    int value;
};

template<typename T>
concept HasValue = requires(T candidate) {
    candidate.value;
};

template<typename T>
concept SmallReturnType = sizeof(T) <= sizeof(int);

template<typename T, typename Bound>
concept ReturnTypeAtMost = sizeof(T) <= sizeof(Bound);

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

int main() {
    ConceptCarrier carrier = {41};
    NoValue missing = {};
    return Positive<3> && !Positive<-2> &&
                   HasValue<ConceptCarrier> && !HasValue<NoValue> &&
                   positive_value<3>() == 3 &&
                   nonpositive_value<-2>() == -2 &&
                   read_value(carrier) == 41 &&
                   read_value_constrained(carrier) == 42 &&
                   read_missing(missing) == 7 &&
                   small_return_type_accepts(SmallSizeProvider{}) == 1 &&
                   small_return_type_accepts(LargeSizeProvider{}) == 0 &&
                   explicit_return_type_accepts(SmallSizeProvider{}) == 1 &&
                   explicit_return_type_accepts(LargeSizeProvider{}) == 0 &&
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
