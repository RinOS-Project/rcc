namespace merged {
struct First {
    int value;
};

int increment(int value) {
    return value + 1;
}
}

namespace merged {
struct Second {
    int value;
};
}

namespace {
struct InternalFirst {
    int value;
};

int add_two(int value) {
    return value + 2;
}
}

namespace {
struct InternalSecond {
    int value;
};
}

namespace api {
inline namespace v1 {
inline namespace v2 {
struct Versioned {
    int value;
};

int add_three(int value) {
    return value + 3;
}

namespace detail {
struct Original {
    int value;
};
}
}
}
}

namespace api::detail {
struct Added {
    int value;
};

int double_value(int value) {
    return value * 2;
}
}

namespace api {
inline namespace compatibility {
namespace exposed {}
}
}

namespace alias_target {}
namespace api {
namespace exposed = alias_target;
}

int main() {
    merged::First first = {2};
    merged::Second second = {3};
    InternalFirst internal_first = {4};
    InternalSecond internal_second = {5};
    api::detail::Original original = {6};
    api::detail::Added added = {7};
    api::v1::detail::Added added_through_outer_inline = {8};
    api::Versioned versioned = {9};
    api::v1::Versioned versioned_through_outer_inline = {10};

    return merged::increment(first.value) == 3 &&
                   internal_first.value + internal_second.value == 9 &&
                   add_two(internal_first.value) == 6 &&
                   versioned.value + versioned_through_outer_inline.value == 19 &&
                   api::detail::double_value(added.value) == 14 &&
                   api::detail::double_value(added_through_outer_inline.value) == 16 &&
                   api::add_three(original.value) == 9 &&
                   second.value == 3
               ? 0
               : 1;
}
