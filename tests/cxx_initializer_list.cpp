#include <initializer_list>

extern "C" void initializer_list_record_constructed(int value);
extern "C" void initializer_list_record_destroyed(int value);
extern "C" void initializer_list_reset_counts(void);
extern "C" int initializer_list_counts_are(int constructed, int destroyed);

struct Tracked {
    int value;

    Tracked(int input) : value(input) {
        initializer_list_record_constructed(value);
    }

    ~Tracked() {
        initializer_list_record_destroyed(value);
    }
};

struct Pair {
    int first;
    int second;
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

static int test_initializer_list_preference(void) {
    int function_choice = choose_overload({5, 6});
    int element_choice = choose_element_type({9, 10});
    int template_element_size = template_list_element_size({11, 12});
    TemplatedConstructorChoice templated_choice{13, 14};
    TemplatePairChoice template_pair_choice{15, 16};
    initializer_list_reset_counts();
    ConstructorChoice constructor_choice{7, 8};
    return function_choice == 1 && element_choice == 3 &&
           template_element_size == sizeof(int) &&
           templated_choice.element_size == sizeof(int) + 2 &&
           template_pair_choice.element_size == sizeof(int) &&
           constructor_choice.selected == 1 &&
           initializer_list_counts_are(1, 0);
}

int main(void) {
    int failures = 0;
    if (!test_argument_lifetime()) failures |= 1;
    if (!test_reference_argument_lifetime()) failures |= 2;
    if (!test_local_lifetime()) failures |= 4;
    if (!test_empty_initializer_list()) failures |= 8;
    if (!test_initializer_list_preference()) failures |= 16;
    return failures;
}
