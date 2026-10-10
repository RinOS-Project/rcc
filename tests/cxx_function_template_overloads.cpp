struct ConversionBaseRoot {};
struct ConversionBaseMiddle : ConversionBaseRoot {};
struct ConversionBaseLeaf : ConversionBaseMiddle {};

enum ConversionFixedByte : unsigned char {
    conversion_fixed_byte_value = 1
};

struct ConversionMemberPointerRoot {
    int value;

    int function(int parameter) const {
        return parameter;
    }
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

int choose_pointer_subsequence_non_template(void*) {
    return 73;
}

int choose_pointer_subsequence_non_template(const void*) {
    return 74;
}

int choose_nested_pointer_qualification(int* const*) {
    return 75;
}

int choose_nested_pointer_qualification(const int* const*) {
    return 76;
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

int choose_nearer_base_pointer_non_template(ConversionBaseRoot*) {
    return 89;
}

int choose_nearer_base_pointer_non_template(ConversionBaseMiddle*) {
    return 90;
}

int choose_fixed_enum_promotion(unsigned char) {
    return 211;
}

int choose_fixed_enum_promotion(int) {
    return 212;
}

int choose_pointer_to_bool_non_template(bool) {
    return 215;
}

int choose_pointer_to_bool_non_template(void*) {
    return 216;
}

int choose_nullptr_to_bool_non_template(bool) {
    return 217;
}

int choose_nullptr_to_bool_non_template(void*) {
    return 218;
}

template<typename T>
int choose_nullptr_to_bool_template(T, bool) {
    return 219;
}

template<typename T>
int choose_nullptr_to_bool_template(T, void*) {
    return 220;
}

template<typename T>
int choose_fixed_enum_template(T, unsigned char) {
    return 213;
}

template<typename T>
int choose_fixed_enum_template(T, int) {
    return 214;
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
int choose_nearer_member_function_pointer(
    T, int (ConversionMemberPointerMiddle::*)(int) const) {
    return 195;
}

template<typename T>
int choose_nearer_member_function_pointer(
    T, int (ConversionMemberPointerLeaf::*)(int) const) {
    return 196;
}

int choose_nearer_member_function_pointer_non_template(
    int (ConversionMemberPointerMiddle::*)(int) const) {
    return 197;
}

int choose_nearer_member_function_pointer_non_template(
    int (ConversionMemberPointerLeaf::*)(int) const) {
    return 198;
}

template<typename T>
int choose_member_pointer_member_type_cv(
    T, int ConversionMemberPointerMiddle::*) {
    return 201;
}

template<typename T>
int choose_member_pointer_member_type_cv(
    T, const int ConversionMemberPointerMiddle::*) {
    return 202;
}

int choose_member_pointer_member_type_cv_non_template(
    int ConversionMemberPointerMiddle::*) {
    return 203;
}

int choose_member_pointer_member_type_cv_non_template(
    const int ConversionMemberPointerMiddle::*) {
    return 204;
}

template<typename T>
int choose_base_over_void(T*, ConversionBaseRoot*) {
    return 83;
}

template<typename T>
int choose_base_over_void(T*, void*) {
    return 84;
}

int choose_base_over_void_non_template(ConversionBaseRoot*) {
    return 97;
}

int choose_base_over_void_non_template(void*) {
    return 98;
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
    int** pointer_to_pointer = &pointer;
    int (ConversionMemberPointerRoot::*root_member_function)(int) const =
        &ConversionMemberPointerRoot::function;
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
                   choose_pointer_subsequence_non_template(pointer) == 73 &&
                   choose_nested_pointer_qualification(pointer_to_pointer) ==
                       75 &&
                   choose_base_over_void_non_template(leaf_pointer) == 97 &&
                   choose_nearer_base(leaf_pointer, leaf_pointer) == 82 &&
                   choose_nearer_base_reference(leaf_pointer, leaf) == 86 &&
                   choose_nearer_base_reference_non_template(leaf) == 88 &&
                   choose_nearer_base_pointer_non_template(leaf_pointer) ==
                       90 &&
                   choose_fixed_enum_promotion(
                       conversion_fixed_byte_value) == 211 &&
                   choose_fixed_enum_template(
                       value, conversion_fixed_byte_value) == 213 &&
                   choose_pointer_to_bool_non_template(pointer) == 216 &&
                   choose_nullptr_to_bool_non_template(nullptr) == 218 &&
                   choose_nullptr_to_bool_template(value, nullptr) == 220 &&
                   !static_cast<bool>(nullptr) &&
                   choose_nearer_member_pointer(
                       0, &ConversionMemberPointerRoot::value) == 191 &&
                   choose_nearer_member_pointer_non_template(
                       &ConversionMemberPointerRoot::value) == 193 &&
                   choose_nearer_member_function_pointer(
                       0, root_member_function) == 195 &&
                   choose_nearer_member_function_pointer_non_template(
                       root_member_function) == 197 &&
                   choose_member_pointer_member_type_cv(
                       0, &ConversionMemberPointerRoot::value) == 201 &&
                   choose_member_pointer_member_type_cv_non_template(
                       &ConversionMemberPointerRoot::value) == 203 &&
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
