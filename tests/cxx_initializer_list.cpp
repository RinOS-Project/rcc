#include <initializer_list>

extern "C" void initializer_list_record_constructed(int value);
extern "C" void initializer_list_record_destroyed(int value);
extern "C" void initializer_list_reset_counts(void);
extern "C" int initializer_list_counts_are(int constructed, int destroyed);

static int initializer_list_clause_evaluation_order;
static int braced_array_destruction_order;

static int initializer_list_clause_value(int value) {
    initializer_list_clause_evaluation_order =
        initializer_list_clause_evaluation_order * 10 + value;
    return value;
}

struct Tracked {
    int value;

    Tracked(int input) : value(input) {
        initializer_list_record_constructed(value);
    }

    ~Tracked() {
        initializer_list_record_destroyed(value);
    }
};

struct BracedArrayTracked {
    int value;

    BracedArrayTracked(int input) : value(input) {
        initializer_list_record_constructed(value);
    }

    ~BracedArrayTracked() {
        initializer_list_record_destroyed(value);
        braced_array_destruction_order =
            braced_array_destruction_order * 100 + value;
    }
};

struct Pair {
    int first;
    int second;
};

struct OrderedInitializerAggregate {
    int first;
    int second;
    int third;
};

struct InitializerListPhaseFallbackValue {
    int value;
};

struct InitializerListPhaseFallbackDerived
    : InitializerListPhaseFallbackValue {};

struct InitializerListPhaseFallbackVirtualDerived
    : virtual InitializerListPhaseFallbackValue {};

struct InitializerListPhaseFallbackConstructor {
    int selected;

    InitializerListPhaseFallbackConstructor(std::initializer_list<int>)
        : selected(1) {}
    InitializerListPhaseFallbackConstructor(
        InitializerListPhaseFallbackValue value)
        : selected(value.value) {}
};

struct InitializerListOnly {
    int size;

    InitializerListOnly(std::initializer_list<int> values)
        : size(static_cast<int>(values.size())) {}
};

struct ExplicitInitializerListOnly {
    int size;

    explicit ExplicitInitializerListOnly(std::initializer_list<int> values)
        : size(static_cast<int>(values.size())) {}
};

struct InitializerListRange {
    bool empty;

    InitializerListRange(std::initializer_list<int> values)
        : empty(values.begin() == values.end()) {}
};

struct EmptyListDefaultChoice {
    int selected;

    EmptyListDefaultChoice() : selected(2) {}
    EmptyListDefaultChoice(std::initializer_list<int>) : selected(1) {}
};

struct EmptyListDefaultArgumentChoice {
    int selected;

    EmptyListDefaultArgumentChoice(int input = 3) : selected(input) {}
    EmptyListDefaultArgumentChoice(std::initializer_list<int>) : selected(1) {}
};

int choose_overload(std::initializer_list<int>) {
    return 1;
}

int choose_overload(Pair) {
    return 2;
}

int choose_element_type(std::initializer_list<int>) {
    return 3;
}

int choose_element_type(std::initializer_list<long>) {
    return 4;
}

int choose_worst_element_conversion(std::initializer_list<short>) {
    return 1;
}

int choose_worst_element_conversion(std::initializer_list<int>) {
    return 2;
}

int choose_braced_array_bound(const int (&)[2]) {
    return 1;
}

int choose_braced_array_bound(const int (&)[3]) {
    return 2;
}

int choose_braced_unknown_array_bound(const int (&&)[]) {
    return 1;
}

int choose_braced_unknown_array_bound(const int (&&)[2]) {
    return 2;
}

static int consume_braced_array_reference(
    const BracedArrayTracked (&values)[2]) {
    return values[0].value == 26 && values[1].value == 27 &&
           initializer_list_counts_are(2, 0);
}

int choose_initializer_list_phase_fallback(std::initializer_list<int>) {
    return 1;
}

int choose_initializer_list_phase_fallback(
    InitializerListPhaseFallbackValue value) {
    return value.value;
}

int choose_initializer_list_derived_rank(
    std::initializer_list<InitializerListPhaseFallbackValue>) {
    return 1;
}

int choose_initializer_list_derived_rank(
    std::initializer_list<InitializerListPhaseFallbackDerived>) {
    return 2;
}

int choose_empty_list_over_scalar(std::initializer_list<int> values) {
    return values.size() == 0 ? 1 : -1;
}

int choose_empty_list_over_scalar(int) {
    return 2;
}

template <typename T>
int template_list_element_size(std::initializer_list<T> values) {
    return values.size() == 2 ? sizeof(T) : 0;
}

struct ConstructorChoice {
    int selected;

    ConstructorChoice(std::initializer_list<int>) : selected(1) {
        initializer_list_record_constructed(1);
    }

    ConstructorChoice(int, int) : selected(2) {
        initializer_list_record_constructed(2);
    }

    template <typename T>
    ConstructorChoice(std::initializer_list<T>) : selected(3) {
        initializer_list_record_constructed(3);
    }
};

struct TemplatedConstructorChoice {
    int element_size;

    template <typename T>
    TemplatedConstructorChoice(std::initializer_list<T> values)
        : element_size(sizeof(T) + values.size()) {}
};

struct TemplatePairChoice {
    int element_size;

    template <typename T>
    TemplatePairChoice(T, T) : element_size(sizeof(T)) {}
};

static int consume(std::initializer_list<Tracked> values) {
    return values.size() == 2 && values.begin()[0].value == 1 &&
           values.begin()[1].value == 2;
}

static int consume_empty(std::initializer_list<int> values) {
    return values.size() == 0 && values.begin() == values.end();
}

static int consume_defaulted(std::initializer_list<int> values = {4, 5}) {
    return static_cast<int>(values.size());
}

static int consume_scalar_default(int value = {6}) {
    return value;
}

struct DefaultedAggregate {
    int value;
};

static int consume_reference_default(const int& value = {7}) {
    return value;
}

static int consume_aggregate_default(DefaultedAggregate value = {4}) {
    return value.value;
}

static int consume_defaulted_tracked(
    std::initializer_list<Tracked> values = {3, 4}) {
    return values.size() == 2 && values.begin()[0].value == 3 &&
           values.begin()[1].value == 4;
}

struct DefaultedInitializerList {
    int size;
    DefaultedInitializerList(
        std::initializer_list<int> values = {7, 8})
        : size(static_cast<int>(values.size())) {}
};

static int consume_reference(const std::initializer_list<Tracked>& values) {
    return values.size() == 2 && values.begin()[0].value == 5 &&
           values.begin()[1].value == 6 &&
           initializer_list_counts_are(2, 0);
}

static int test_argument_lifetime(void) {
    int contents;
    initializer_list_reset_counts();
    contents = consume({1, 2});
    return contents && initializer_list_counts_are(2, 2);
}

static int test_reference_argument_lifetime(void) {
    int contents;
    initializer_list_reset_counts();
    contents = consume_reference({5, 6});
    return contents && initializer_list_counts_are(2, 2);
}

static int test_braced_array_reference_lifetime(void) {
    int contents;
    braced_array_destruction_order = 0;
    initializer_list_reset_counts();
    contents = consume_braced_array_reference({26, 27});
    return contents && initializer_list_counts_are(2, 2) &&
           braced_array_destruction_order == 2726;
}

static int test_local_lifetime(void) {
    int contents = 0;
    initializer_list_reset_counts();
    {
        std::initializer_list<Tracked> values{3, 4};
        contents = values.size() == 2 && values.begin()[0].value == 3 &&
                   values.begin()[1].value == 4 &&
                   initializer_list_counts_are(2, 0);
    }
    return contents && initializer_list_counts_are(2, 2);
}

static int test_empty_initializer_list(void) {
    std::initializer_list<int> values{};
    return values.size() == 0 && values.begin() == values.end();
}

static int test_empty_initializer_list_argument(void) {
    return consume_empty({});
}

static int test_braced_initializer_list_default_arguments(void) {
    DefaultedInitializerList value;
    auto lambda = [](int parameter = {11}) { return parameter; };
    auto tracked_lambda = [](std::initializer_list<Tracked> values = {8, 9}) {
        return values.size() == 2 && values.begin()[0].value == 8 &&
               values.begin()[1].value == 9;
    };
    int tracked_result;
    bool lambda_tracked_result;
    initializer_list_reset_counts();
    tracked_result = consume_defaulted_tracked();
    if (!tracked_result || !initializer_list_counts_are(2, 2)) return 0;
    initializer_list_reset_counts();
    lambda_tracked_result = tracked_lambda() && tracked_lambda();
    return consume_defaulted() == 2 && value.size == 2 &&
           consume_defaulted({9}) == 1 && consume_scalar_default() == 6 &&
           consume_reference_default() == 7 &&
           consume_aggregate_default() == 4 &&
           lambda() == 11 && lambda({12}) == 12 &&
           lambda_tracked_result && initializer_list_counts_are(4, 4);
}

static int test_initializer_list_preference(void) {
    int function_choice = choose_overload({5, 6});
    int element_choice = choose_element_type({9, 10});
    short short_source = 1;
    char char_source = 2;
    int worst_element_conversion = choose_worst_element_conversion(
        {short_source, char_source});
    int braced_array_bound = choose_braced_array_bound({30, 31});
    int braced_unknown_array_short = choose_braced_unknown_array_bound({32});
    int braced_unknown_array_exact = choose_braced_unknown_array_bound(
        {33, 34});
    int empty_list_function_choice = choose_empty_list_over_scalar({});
    int template_element_size = template_list_element_size({11, 12});
    TemplatedConstructorChoice templated_choice{13, 14};
    TemplatePairChoice template_pair_choice{15, 16};
    InitializerListOnly copied_list_only = {17, 18};
    ExplicitInitializerListOnly direct_explicit_list_only{19, 20};
    InitializerListRange empty_range{};
    InitializerListRange nonempty_range{21};
    EmptyListDefaultChoice empty_list_default_choice{};
    EmptyListDefaultArgumentChoice default_argument_choice{};
    InitializerListPhaseFallbackDerived constructor_fallback_value{{22}};
    InitializerListPhaseFallbackConstructor constructor_fallback{
        constructor_fallback_value};
    InitializerListPhaseFallbackDerived function_fallback_value{{23}};
    int function_fallback = choose_initializer_list_phase_fallback(
        {function_fallback_value});
    int initializer_list_element_rank = choose_initializer_list_derived_rank(
        {function_fallback_value});
    InitializerListPhaseFallbackVirtualDerived virtual_fallback_value{};
    virtual_fallback_value.value = 24;
    InitializerListPhaseFallbackConstructor virtual_constructor_fallback{
        virtual_fallback_value};
    int virtual_function_fallback = choose_initializer_list_phase_fallback(
        {virtual_fallback_value});
    initializer_list_reset_counts();
    ConstructorChoice constructor_choice{7, 8};
    return function_choice == 1 && element_choice == 3 &&
           worst_element_conversion == 2 &&
           braced_array_bound == 1 &&
           braced_unknown_array_short == 1 &&
           braced_unknown_array_exact == 2 &&
           empty_list_function_choice == 1 &&
           template_element_size == sizeof(int) &&
           templated_choice.element_size == sizeof(int) + 2 &&
           template_pair_choice.element_size == sizeof(int) &&
           copied_list_only.size == 2 &&
           direct_explicit_list_only.size == 2 &&
           empty_range.empty && !nonempty_range.empty &&
           empty_list_default_choice.selected == 2 &&
           default_argument_choice.selected == 3 &&
           constructor_fallback.selected == 22 &&
           function_fallback == 23 &&
           initializer_list_element_rank == 2 &&
           virtual_constructor_fallback.selected == 24 &&
           virtual_function_fallback == 24 &&
           constructor_choice.selected == 1 &&
           initializer_list_counts_are(1, 0);
}

static int test_initializer_clause_evaluation_order(void) {
    initializer_list_clause_evaluation_order = 0;
    OrderedInitializerAggregate aggregate{
        initializer_list_clause_value(1),
        initializer_list_clause_value(2),
        initializer_list_clause_value(3)};
    if (initializer_list_clause_evaluation_order != 123 ||
        aggregate.first != 1 || aggregate.second != 2 ||
        aggregate.third != 3) return 0;

    initializer_list_clause_evaluation_order = 0;
    int array[3]{initializer_list_clause_value(1),
                 initializer_list_clause_value(2),
                 initializer_list_clause_value(3)};
    if (initializer_list_clause_evaluation_order != 123 ||
        array[0] != 1 || array[1] != 2 || array[2] != 3) return 0;

    initializer_list_clause_evaluation_order = 0;
    std::initializer_list<int> values{
        initializer_list_clause_value(1),
        initializer_list_clause_value(2),
        initializer_list_clause_value(3)};
    return initializer_list_clause_evaluation_order == 123 &&
           values.size() == 3 && values.begin()[0] == 1 &&
           values.begin()[1] == 2 && values.begin()[2] == 3;
}

int main(void) {
    int failures = 0;
    if (!test_argument_lifetime()) failures |= 1;
    if (!test_reference_argument_lifetime()) failures |= 2;
    if (!test_braced_array_reference_lifetime()) failures |= 256;
    if (!test_local_lifetime()) failures |= 4;
    if (!test_empty_initializer_list()) failures |= 8;
    if (!test_empty_initializer_list_argument()) failures |= 32;
    if (!test_braced_initializer_list_default_arguments()) failures |= 64;
    if (!test_initializer_list_preference()) failures |= 16;
    if (!test_initializer_clause_evaluation_order()) failures |= 128;
    return failures;
}
