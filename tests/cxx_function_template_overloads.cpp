struct ConversionBaseRoot {};
struct ConversionBaseMiddle : ConversionBaseRoot {};
struct ConversionBaseLeaf : ConversionBaseMiddle {};

struct ConversionMemberPointerRoot {
    int value;
};

struct ConversionMemberPointerMiddle : ConversionMemberPointerRoot {};
struct ConversionMemberPointerLeaf : ConversionMemberPointerMiddle {};

struct ConversionValueRoot {
    int root_value;
};

struct ConversionValuePrefix {
    int prefix_value;
};

struct ConversionValueMiddle : ConversionValuePrefix,
                               ConversionValueRoot {
    int middle_value;
};

struct ConversionValueLeaf : ConversionValueMiddle {
    int leaf_value;
};

int conversion_value_cleanup_count = 0;

struct ConversionValueCleanupRoot {
    int root_value;
};

struct ConversionValueCleanupPrefix {
    int prefix_value;
};

struct ConversionValueCleanupLeaf : ConversionValueCleanupPrefix,
                                    ConversionValueCleanupRoot {
    int leaf_value;
    ~ConversionValueCleanupLeaf() {
        ++conversion_value_cleanup_count;
    }
};

template<typename T>
int choose_template(T) {
    return 10;
}

template<typename T>
int choose_template(T* value) {
    return *value + 20;
}

template<typename T>
int choose_fixed_over_pack(T) {
    return 110;
}

template<typename... T>
int choose_fixed_over_pack(T...) {
    return 120;
}

template<typename T, typename... Rest>
int choose_fixed_prefix_over_pack(T, Rest...) {
    return 130;
}

template<typename... T>
int choose_fixed_prefix_over_pack(T...) {
    return 140;
}

template<typename T, typename... Rest>
int choose_nonpack_over_empty_pack(T, Rest...) {
    return 150;
}

template<typename T>
int choose_nonpack_over_empty_pack(T) {
    return 160;
}

template<typename T>
T forward_template(T value) {
    return value;
}

template<typename T>
int choose_conversion_before_partial_order(T, int) {
    return 10;
}

template<typename T>
int choose_conversion_before_partial_order(T*, long) {
    return 20;
}

template<typename T>
int choose_cv_qualification(const T*) {
    return 30;
}

template<typename T>
int choose_cv_qualification(const volatile T*) {
    return 40;
}

template<typename T>
int choose_reference_binding(const T&) {
    return 51;
}

template<typename T>
int choose_reference_binding(const T&&) {
    return 52;
}

template<typename T>
int choose_reference_cv_category(T&) {
    return 171;
}

template<typename T>
int choose_reference_cv_category(const T&) {
    return 172;
}

template<typename T>
int choose_rvalue_cv_category(T&&) {
    return 173;
}

template<typename T>
int choose_rvalue_cv_category(const T&&) {
    return 174;
}

template<typename T>
int choose_pointer_bool(T*, bool) {
    return 61;
}

template<typename T>
int choose_pointer_bool(T*, void*) {
    return 62;
}

template<typename T>
int choose_pointer_subsequence(T*, void*) {
    return 71;
}

template<typename T>
int choose_pointer_subsequence(T*, const void*) {
    return 72;
}

template<typename T>
int choose_nearer_base(T*, ConversionBaseRoot*) {
    return 81;
}

template<typename T>
int choose_nearer_base(T*, ConversionBaseMiddle*) {
    return 82;
}

template<typename T>
int choose_nearer_base_reference(T*, ConversionBaseRoot&) {
    return 85;
}

template<typename T>
int choose_nearer_base_reference(T*, ConversionBaseMiddle&) {
    return 86;
}

int choose_nearer_base_reference_non_template(ConversionBaseRoot&) {
    return 87;
}

int choose_nearer_base_reference_non_template(ConversionBaseMiddle&) {
    return 88;
}

template<typename T>
int choose_nearer_member_pointer(
    T, int ConversionMemberPointerMiddle::*) {
    return 191;
}

template<typename T>
int choose_nearer_member_pointer(
    T, int ConversionMemberPointerLeaf::*) {
    return 192;
}

int choose_nearer_member_pointer_non_template(
    int ConversionMemberPointerMiddle::*) {
    return 193;
}

int choose_nearer_member_pointer_non_template(
    int ConversionMemberPointerLeaf::*) {
    return 194;
}

template<typename T>
int choose_base_over_void(T*, ConversionBaseRoot*) {
    return 83;
}

template<typename T>
int choose_base_over_void(T*, void*) {
    return 84;
}

template<typename T>
int choose_nearer_base_value(T*, ConversionValueRoot value) {
    return value.root_value == 41 ? 91 : -91;
}

template<typename T>
int choose_nearer_base_value(T*, ConversionValueMiddle value) {
    return value.prefix_value == 37 && value.root_value == 41 &&
                   value.middle_value == 43
               ? 92
               : -92;
}

int choose_nearer_base_value_non_template(ConversionValueRoot value) {
    return value.root_value == 41 ? 93 : -93;
}

int choose_nearer_base_value_non_template(ConversionValueMiddle value) {
    return value.prefix_value == 37 && value.root_value == 41 &&
                   value.middle_value == 43
               ? 94
               : -94;
}

static ConversionValueLeaf make_conversion_value_leaf(void) {
    ConversionValueLeaf value;
    value.prefix_value = 37;
    value.root_value = 41;
    value.middle_value = 43;
    value.leaf_value = 47;
    return value;
}

static ConversionValueCleanupLeaf make_conversion_value_cleanup_leaf(
    void) noexcept {
    return ConversionValueCleanupLeaf{{49}, {53}, 59};
}

static int consume_conversion_value_cleanup_root(
    ConversionValueCleanupRoot value) noexcept {
    return value.root_value;
}

int main(void) {
    int value = 5;
    int* pointer = &value;
    const int const_value = 6;
    ConversionBaseLeaf leaf;
    ConversionBaseLeaf* leaf_pointer = &leaf;
    ConversionValueLeaf value_leaf;
    ConversionValueLeaf* value_leaf_pointer = &value_leaf;
    value_leaf.prefix_value = 37;
    value_leaf.root_value = 41;
    value_leaf.middle_value = 43;
    value_leaf.leaf_value = 47;
    ConversionValueRoot root_copy = value_leaf;
    int cleanup_count_before = conversion_value_cleanup_count;
    int cleanup_root_value = consume_conversion_value_cleanup_root(
        make_conversion_value_cleanup_leaf());
    short priority = 0;
    return choose_template(&value) == 25 &&
                   choose_template(value) == 10 &&
                   choose_fixed_over_pack(value) == 110 &&
                   choose_fixed_prefix_over_pack(value, value) == 130 &&
                   choose_nonpack_over_empty_pack(value) == 160 &&
                   forward_template(choose_template(&value)) == 25 &&
                   choose_conversion_before_partial_order(&value, priority) == 10 &&
                   choose_cv_qualification(pointer) == 30 &&
                   choose_reference_binding(value) == 51 &&
                   choose_reference_binding(0) == 52 &&
                   choose_reference_cv_category(value) == 171 &&
                   choose_reference_cv_category(const_value) == 172 &&
                   choose_rvalue_cv_category(0) == 173 &&
                   choose_rvalue_cv_category(
                       static_cast<const int&&>(const_value)) == 174 &&
                   choose_pointer_bool(pointer, pointer) == 62 &&
                   choose_pointer_subsequence(pointer, pointer) == 71 &&
                   choose_nearer_base(leaf_pointer, leaf_pointer) == 82 &&
                   choose_nearer_base_reference(leaf_pointer, leaf) == 86 &&
                   choose_nearer_base_reference_non_template(leaf) == 88 &&
                   choose_nearer_member_pointer(
                       0, &ConversionMemberPointerRoot::value) == 191 &&
                   choose_nearer_member_pointer_non_template(
                       &ConversionMemberPointerRoot::value) == 193 &&
                   choose_base_over_void(leaf_pointer, leaf_pointer) == 83 &&
                   choose_nearer_base_value(
                       value_leaf_pointer, value_leaf) == 92 &&
                   choose_nearer_base_value_non_template(value_leaf) == 94 &&
                   choose_nearer_base_value_non_template(
                       make_conversion_value_leaf()) == 94 &&
                   root_copy.root_value == 41 &&
                   cleanup_root_value == 53 &&
                   conversion_value_cleanup_count == cleanup_count_before + 1
               ? 0
               : 1;
}
