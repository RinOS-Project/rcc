struct ConversionBaseRoot {};
struct ConversionBaseMiddle : ConversionBaseRoot {};
struct ConversionBaseLeaf : ConversionBaseMiddle {};

enum ConversionFixedByte : unsigned char {
    conversion_fixed_byte_value = 1
};

using ConversionThrowingFunctionPointer = void (*)();
using ConversionNoexceptFunctionPointer = void (*)() noexcept;
using ConversionExplicitThrowingFunctionPointer = void (*)() noexcept(false);
using ConversionExplicitNoexceptFunctionPointer = void (*)() noexcept(true);
using ConversionNoexceptIntPointerFunction = int (*)(int*) noexcept;
using ConversionThrowingIntPointerFunction = int (*)(int*);
using ConversionIntFunction = int (*)(int);
using ConversionIntPairFunction = int (*)(int, int);

void conversion_noexcept_function() noexcept {}
void conversion_explicit_throwing_function() noexcept(false) {}
void conversion_explicit_noexcept_function() noexcept(true) {}
void conversion_overloaded_noexcept_function(int) {}
void conversion_overloaded_noexcept_function() noexcept {}

template<typename T>
int conversion_function_template_overload(T) noexcept {
    return 1;
}

template<typename T>
int conversion_function_template_overload(T*) noexcept {
    return 2;
}

template<typename T>
int conversion_explicit_noexcept_function_template(T*) noexcept(true) {
    return 3;
}

template<typename T>
int conversion_dependent_noexcept_function_template(T*)
    noexcept(sizeof(T) == sizeof(int)) {
    return 4;
}

template<typename T>
int conversion_dependent_throwing_function_template(T*)
    noexcept(sizeof(T) != sizeof(int)) {
    return 5;
}

int conversion_non_template_preferred(int*) noexcept {
    return 6;
}

template<typename T>
int conversion_non_template_preferred(T*) noexcept {
    return 7;
}

int conversion_noexcept_template_rank(int*) {
    return 91;
}

template<typename T>
int conversion_noexcept_template_rank(T*) noexcept {
    return 92;
}

namespace ConversionFunctionTemplateNamespace {
template<typename T>
int select(T*) noexcept {
    return 8;
}
}

namespace ConversionFunctionTemplateImportedNamespace {
template<typename T>
int select_via_directive(T*) noexcept {
    return 81;
}

template<typename T>
int select_via_declaration(T*) noexcept {
    return 82;
}
}

namespace ConversionFunctionTemplateUsingDirectiveNamespace {
using namespace ConversionFunctionTemplateImportedNamespace;
}

namespace ConversionFunctionTemplateUsingDeclarationNamespace {
using ConversionFunctionTemplateImportedNamespace::select_via_declaration;
}

template<typename... T>
int conversion_function_template_pack(T...) {
    return 6;
}

struct ConversionMemberPointerRoot {
    int value;

    int function(int parameter) const {
        return parameter;
    }

    void noexcept_function() noexcept {}

    void throwing_function() {}
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

struct ConversionValueVirtualRoot {
    int root_value;
};

struct ConversionValueVirtualPrefix {
    int prefix_value;
};

struct ConversionValueVirtualPayload : ConversionValueVirtualPrefix,
                                       ConversionValueVirtualRoot {
    int payload_value;
};

struct ConversionValueVirtualMiddle : virtual ConversionValueVirtualPayload {
    int middle_value;
};

struct ConversionValueVirtualLeaf : ConversionValueVirtualMiddle {
    int leaf_value;
};

int conversion_value_cleanup_count = 0;

struct ConversionValueVirtualCleanupRoot {
    int root_value;
};

struct ConversionValueVirtualCleanupMiddle :
    virtual ConversionValueVirtualCleanupRoot {
    int middle_value;
};

struct ConversionValueVirtualCleanupLeaf :
    ConversionValueVirtualCleanupMiddle {
    int leaf_value;
    ~ConversionValueVirtualCleanupLeaf() {
        ++conversion_value_cleanup_count;
    }
};

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

template<typename... T>
int choose_pack_pointer_partial_order(T...) {
    return 145;
}

template<typename T, typename... Rest>
int choose_pack_pointer_partial_order(T*, Rest...) {
    return 146;
}

template<typename T, typename... Rest>
int choose_nonpack_over_empty_pack(T, Rest...) {
    return 150;
}

template<typename T>
int choose_nonpack_over_empty_pack(T) {
    return 160;
}

int verify_function_template_pointer_overload() {
    ConversionNoexceptIntPointerFunction selected =
        conversion_function_template_overload;
    ConversionNoexceptIntPointerFunction explicit_noexcept =
        conversion_explicit_noexcept_function_template;
    ConversionNoexceptIntPointerFunction dependent_noexcept =
        conversion_dependent_noexcept_function_template;
    ConversionThrowingIntPointerFunction dependent_throwing =
        conversion_dependent_throwing_function_template;
    ConversionNoexceptIntPointerFunction non_template_preferred =
        conversion_non_template_preferred;
    ConversionNoexceptIntPointerFunction namespaced =
        ConversionFunctionTemplateNamespace::select;
    ConversionIntPairFunction packed = conversion_function_template_pack;
    ConversionIntFunction fixed_over_pack = choose_fixed_over_pack;
    ConversionIntPairFunction fixed_prefix_over_pack =
        choose_fixed_prefix_over_pack;
    return selected(nullptr) == 2 && explicit_noexcept(nullptr) == 3 &&
           dependent_noexcept(nullptr) == 4 &&
           dependent_throwing(nullptr) == 5 &&
           non_template_preferred(nullptr) == 6 &&
           namespaced(nullptr) == 8 &&
           packed(1, 2) == 6 &&
           fixed_over_pack(1) == 110 && fixed_prefix_over_pack(1, 2) == 130;
}

int verify_noexcept_function_template_rank() {
    ConversionNoexceptIntPointerFunction nonthrowing =
        conversion_noexcept_template_rank;
    ConversionThrowingIntPointerFunction throwing =
        conversion_noexcept_template_rank;
    return nonthrowing(nullptr) == 92 && throwing(nullptr) == 91;
}

int verify_function_template_using_lookup() {
    ConversionNoexceptIntPointerFunction using_directive =
        ConversionFunctionTemplateUsingDirectiveNamespace::select_via_directive;
    ConversionNoexceptIntPointerFunction using_declaration =
        ConversionFunctionTemplateUsingDeclarationNamespace::select_via_declaration;
    return using_directive(nullptr) == 81 &&
           using_declaration(nullptr) == 82;
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

int choose_reference_qualification(const int&) {
    return 231;
}

int choose_reference_qualification(const volatile int&) {
    return 232;
}

int choose_reference_pointer_qualification(const int* const&) {
    return 245;
}

int choose_reference_pointer_qualification(int* const&) {
    return 246;
}

template<typename T>
int choose_reference_qualification_template(T, const int&) {
    return 241;
}

template<typename T>
int choose_reference_qualification_template(T, const volatile int&) {
    return 242;
}

template<typename T>
int choose_reference_pointer_qualification_template(
    T, const int* const&) {
    return 243;
}

template<typename T>
int choose_reference_pointer_qualification_template(T, int* const&) {
    return 244;
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

struct ConversionPointerTrailingSequence {
    int* value;
    operator int*() const { return value; }
};

int choose_same_conversion_trailing_sequence(int*) {
    return 241;
}

int choose_same_conversion_trailing_sequence(const int*) {
    return 242;
}

template<typename Tag>
int choose_same_conversion_trailing_sequence_template(Tag, int*) {
    return 243;
}

template<typename Tag>
int choose_same_conversion_trailing_sequence_template(Tag, const int*) {
    return 244;
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

int choose_array_bound_reference(int (&value)[]) {
    return value[0] + 10;
}

int choose_array_bound_reference(int (&value)[1]) {
    return value[0] + 20;
}

template<typename Element>
int choose_array_bound_reference_template(Element (&value)[]) {
    return value[0] + 30;
}

template<typename Element>
int choose_array_bound_reference_template(Element (&value)[1]) {
    return value[0] + 40;
}

int choose_array_bound_reference_cv(int (&value)[]) {
    return value[0] + 50;
}

int choose_array_bound_reference_cv(const int (&value)[1]) {
    return value[0] + 60;
}

template<typename Element>
int choose_array_bound_reference_cv_template(Element (&value)[]) {
    return value[0] + 70;
}

template<typename Element>
int choose_array_bound_reference_cv_template(const Element (&value)[1]) {
    return value[0] + 80;
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

int choose_cv_nearer_base_reference(const ConversionBaseRoot&) {
    return 102;
}

int choose_cv_nearer_base_reference(ConversionBaseMiddle&) {
    return 103;
}

template<typename T>
int choose_cv_nearer_base_reference_template(
    T*, const ConversionBaseRoot&) {
    return 104;
}

template<typename T>
int choose_cv_nearer_base_reference_template(
    T*, ConversionBaseMiddle&) {
    return 105;
}

int choose_cv_derived_base_reference(ConversionBaseRoot&) {
    return 106;
}

int choose_cv_derived_base_reference(const ConversionBaseMiddle&) {
    return 107;
}

template<typename T>
int choose_cv_derived_base_reference_template(
    T*, ConversionBaseRoot&) {
    return 108;
}

template<typename T>
int choose_cv_derived_base_reference_template(
    T*, const ConversionBaseMiddle&) {
    return 109;
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

int choose_derived_base_qualification(ConversionBaseRoot*) {
    return 99;
}

int choose_derived_base_qualification(const ConversionBaseRoot*) {
    return 100;
}

int choose_cv_nearer_base_pointer(const ConversionBaseRoot*) {
    return 111;
}

int choose_cv_nearer_base_pointer(ConversionBaseMiddle*) {
    return 112;
}

int choose_cv_incomparable_nearer_base_pointer(
    const ConversionBaseMiddle*) {
    return 115;
}

int choose_cv_incomparable_nearer_base_pointer(
    volatile ConversionBaseRoot*) {
    return 116;
}

template<typename T>
int choose_derived_base_qualification_template(
    T*, ConversionBaseRoot*) {
    return 101;
}

template<typename T>
int choose_derived_base_qualification_template(
    T*, const ConversionBaseRoot*) {
    return 102;
}

template<typename T>
int choose_cv_nearer_base_pointer_template(
    T*, const ConversionBaseRoot*) {
    return 113;
}

template<typename T>
int choose_cv_nearer_base_pointer_template(
    T*, ConversionBaseMiddle*) {
    return 114;
}

template<typename T>
int choose_cv_incomparable_nearer_base_pointer_template(
    T*, const ConversionBaseMiddle*) {
    return 117;
}

template<typename T>
int choose_cv_incomparable_nearer_base_pointer_template(
    T*, volatile ConversionBaseRoot*) {
    return 118;
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

int choose_noexcept_function_pointer(
    ConversionThrowingFunctionPointer) {
    return 221;
}

int choose_noexcept_function_pointer(
    ConversionNoexceptFunctionPointer) {
    return 222;
}

template<typename T>
int choose_noexcept_function_pointer_template(
    T, ConversionThrowingFunctionPointer) {
    return 223;
}

template<typename T>
int choose_noexcept_function_pointer_template(
    T, ConversionNoexceptFunctionPointer) {
    return 224;
}

namespace ConversionFunctionOverloadNamespace {
void conversion_overloaded_noexcept_function(int) {}
void conversion_overloaded_noexcept_function() noexcept {}

int verify_function_pointer_overload_context() {
    ConversionNoexceptFunctionPointer selected =
        conversion_overloaded_noexcept_function;
    ConversionNoexceptFunctionPointer global =
        ::conversion_overloaded_noexcept_function;
    return selected != global &&
           ::choose_noexcept_function_pointer(
               conversion_overloaded_noexcept_function) == 222 &&
           ::choose_noexcept_function_pointer_template(
               1, conversion_overloaded_noexcept_function) == 224;
}
}

namespace ConversionFunctionImportedNamespace {
void imported_overloaded_function(int) {}
void imported_overloaded_function() noexcept {}
}

namespace ConversionFunctionUsingDeclarationNamespace {
using ConversionFunctionImportedNamespace::imported_overloaded_function;

int verify_function_pointer_overload_context() {
    ConversionNoexceptFunctionPointer selected =
        imported_overloaded_function;
    ConversionNoexceptFunctionPointer qualified =
        ::ConversionFunctionImportedNamespace::imported_overloaded_function;
    return selected == qualified &&
           ::choose_noexcept_function_pointer(
               imported_overloaded_function) == 222 &&
           ::choose_noexcept_function_pointer_template(
               1, imported_overloaded_function) == 224;
}
}

namespace ConversionFunctionUsingDirectiveNamespace {
using namespace ConversionFunctionImportedNamespace;

int verify_function_pointer_overload_context() {
    ConversionNoexceptFunctionPointer selected =
        imported_overloaded_function;
    ConversionNoexceptFunctionPointer qualified =
        ::ConversionFunctionImportedNamespace::imported_overloaded_function;
    return selected == qualified &&
           ::choose_noexcept_function_pointer(
               imported_overloaded_function) == 222 &&
           ::choose_noexcept_function_pointer_template(
               1, imported_overloaded_function) == 224;
}
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
int choose_member_function_pointer_noexcept(
    T, void (ConversionMemberPointerMiddle::*)() noexcept) {
    return 205;
}

template<typename T>
int choose_member_function_pointer_noexcept(
    T, void (ConversionMemberPointerMiddle::*)()) {
    return 206;
}

int choose_member_function_pointer_noexcept_non_template(
    void (ConversionMemberPointerMiddle::*)() noexcept) {
    return 207;
}

int choose_member_function_pointer_noexcept_non_template(
    void (ConversionMemberPointerMiddle::*)()) {
    return 208;
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

static int consume_conversion_value_virtual_root(
    ConversionValueVirtualRoot value) noexcept {
    return value.root_value;
}

static ConversionValueCleanupLeaf make_conversion_value_cleanup_leaf(
    void) noexcept {
    return ConversionValueCleanupLeaf{{49}, {53}, 59};
}

static int consume_conversion_value_cleanup_root(
    ConversionValueCleanupRoot value) noexcept {
    return value.root_value;
}

static ConversionValueVirtualCleanupLeaf
make_conversion_value_virtual_cleanup_leaf(void) noexcept {
    ConversionValueVirtualCleanupLeaf value;
    value.root_value = 73;
    value.middle_value = 79;
    value.leaf_value = 83;
    return value;
}

static int consume_conversion_value_virtual_cleanup_root(
    ConversionValueVirtualCleanupRoot value) noexcept {
    return value.root_value;
}

int main(void) {
    int value = 5;
    int* pointer = &value;
    ConversionPointerTrailingSequence conversion_pointer{&value};
    int** pointer_to_pointer = &pointer;
    int array_value[1] = { value };
    ConversionNoexceptFunctionPointer noexcept_pointer =
        &conversion_noexcept_function;
    ConversionExplicitThrowingFunctionPointer explicit_throwing_pointer =
        &conversion_explicit_throwing_function;
    ConversionExplicitNoexceptFunctionPointer explicit_noexcept_pointer =
        &conversion_explicit_noexcept_function;
    int (ConversionMemberPointerRoot::*root_member_function)(int) const =
        &ConversionMemberPointerRoot::function;
    void (ConversionMemberPointerRoot::*root_noexcept_member_function)()
        noexcept = &ConversionMemberPointerRoot::noexcept_function;
    void (ConversionMemberPointerRoot::*root_throwing_member_function)() =
        &ConversionMemberPointerRoot::throwing_function;
    const int const_value = 6;
    ConversionBaseLeaf leaf;
    ConversionBaseLeaf* leaf_pointer = &leaf;
    ConversionValueLeaf value_leaf;
    ConversionValueLeaf* value_leaf_pointer = &value_leaf;
    ConversionValueVirtualLeaf virtual_value_leaf;
    value_leaf.prefix_value = 37;
    value_leaf.root_value = 41;
    value_leaf.middle_value = 43;
    value_leaf.leaf_value = 47;
    virtual_value_leaf.root_value = 61;
    virtual_value_leaf.middle_value = 67;
    virtual_value_leaf.leaf_value = 71;
    ConversionValueRoot root_copy = value_leaf;
    int cleanup_count_before = conversion_value_cleanup_count;
    int cleanup_root_value = consume_conversion_value_cleanup_root(
        make_conversion_value_cleanup_leaf());
    int virtual_cleanup_count_before = conversion_value_cleanup_count;
    int virtual_cleanup_root_value =
        consume_conversion_value_virtual_cleanup_root(
            make_conversion_value_virtual_cleanup_leaf());
    if (consume_conversion_value_virtual_root(virtual_value_leaf) != 61) {
        return 21;
    }
    if (virtual_cleanup_root_value != 73) return 22;
    if (conversion_value_cleanup_count !=
        virtual_cleanup_count_before + 2) {
        return 23;
    }
    short priority = 0;
    return choose_template(&value) == 25 &&
                   choose_template(value) == 10 &&
                   choose_fixed_over_pack(value) == 110 &&
                   choose_fixed_prefix_over_pack(value, value) == 130 &&
                   choose_pack_pointer_partial_order(pointer, value) == 146 &&
                   choose_nonpack_over_empty_pack(value) == 160 &&
                   forward_template(choose_template(&value)) == 25 &&
                   choose_conversion_before_partial_order(&value, priority) == 10 &&
                   choose_cv_qualification(pointer) == 30 &&
                   choose_reference_binding(value) == 51 &&
                   choose_reference_binding(0) == 52 &&
                   choose_reference_cv_category(value) == 171 &&
                   choose_reference_cv_category(const_value) == 172 &&
                   choose_reference_qualification(value) == 231 &&
                   choose_reference_pointer_qualification(pointer) == 246 &&
                   choose_reference_qualification_template(value, value) ==
                       241 &&
                   choose_reference_pointer_qualification_template(
                       value, pointer) == 244 &&
                   choose_rvalue_cv_category(0) == 173 &&
                   choose_rvalue_cv_category(
                       static_cast<const int&&>(const_value)) == 174 &&
                   choose_pointer_bool(pointer, pointer) == 62 &&
                   choose_pointer_subsequence(pointer, pointer) == 71 &&
                   choose_pointer_subsequence_non_template(pointer) == 73 &&
                   choose_same_conversion_trailing_sequence(
                       conversion_pointer) == 241 &&
                   choose_same_conversion_trailing_sequence_template(
                       value, conversion_pointer) == 243 &&
                   choose_nested_pointer_qualification(pointer_to_pointer) ==
                       75 &&
                   choose_array_bound_reference(array_value) == 25 &&
                   choose_array_bound_reference_template(array_value) == 45 &&
                   choose_array_bound_reference_cv(array_value) == 65 &&
                   choose_array_bound_reference_cv_template(array_value) ==
                       85 &&
                   choose_base_over_void_non_template(leaf_pointer) == 97 &&
                   choose_nearer_base(leaf_pointer, leaf_pointer) == 82 &&
                   choose_nearer_base_reference(leaf_pointer, leaf) == 86 &&
                   choose_cv_nearer_base_reference(leaf) == 103 &&
                   choose_cv_nearer_base_reference_template(
                       leaf_pointer, leaf) == 105 &&
                   choose_cv_derived_base_reference(leaf) == 107 &&
                   choose_cv_derived_base_reference_template(
                       leaf_pointer, leaf) == 109 &&
                   choose_nearer_base_reference_non_template(leaf) == 88 &&
                   choose_nearer_base_pointer_non_template(leaf_pointer) ==
                       90 &&
                   choose_derived_base_qualification(leaf_pointer) == 99 &&
                   choose_derived_base_qualification_template(
                       pointer, leaf_pointer) == 101 &&
                   choose_cv_nearer_base_pointer(leaf_pointer) == 112 &&
                   choose_cv_nearer_base_pointer_template(
                       pointer, leaf_pointer) == 114 &&
                   choose_cv_incomparable_nearer_base_pointer(
                       leaf_pointer) == 115 &&
                   choose_cv_incomparable_nearer_base_pointer_template(
                       pointer, leaf_pointer) == 117 &&
                   choose_fixed_enum_promotion(
                       conversion_fixed_byte_value) == 211 &&
                   choose_fixed_enum_template(
                       value, conversion_fixed_byte_value) == 213 &&
                   choose_pointer_to_bool_non_template(pointer) == 216 &&
                   choose_nullptr_to_bool_non_template(nullptr) == 218 &&
                   choose_nullptr_to_bool_template(value, nullptr) == 220 &&
                   !static_cast<bool>(nullptr) &&
                   choose_noexcept_function_pointer(noexcept_pointer) == 222 &&
                   choose_noexcept_function_pointer_template(
                       value, noexcept_pointer) == 224 &&
                   choose_noexcept_function_pointer(
                       conversion_noexcept_function) == 222 &&
                   choose_noexcept_function_pointer_template(
                       value, conversion_noexcept_function) == 224 &&
                   choose_noexcept_function_pointer(
                       conversion_overloaded_noexcept_function) == 222 &&
                   choose_noexcept_function_pointer_template(
                       value,
                       conversion_overloaded_noexcept_function) == 224 &&
                   ConversionFunctionOverloadNamespace::
                       verify_function_pointer_overload_context() &&
                   ConversionFunctionUsingDeclarationNamespace::
                       verify_function_pointer_overload_context() &&
                   ConversionFunctionUsingDirectiveNamespace::
                       verify_function_pointer_overload_context() &&
                   verify_function_template_pointer_overload() &&
                   verify_function_template_using_lookup() &&
                   verify_noexcept_function_template_rank() &&
                   choose_noexcept_function_pointer(
                       explicit_noexcept_pointer) == 222 &&
                   choose_noexcept_function_pointer_template(
                       value, explicit_noexcept_pointer) == 224 &&
                   choose_noexcept_function_pointer(
                       explicit_throwing_pointer) == 221 &&
                   choose_noexcept_function_pointer_template(
                       value, explicit_throwing_pointer) == 223 &&
                   choose_noexcept_function_pointer(
                       conversion_explicit_noexcept_function) == 222 &&
                   choose_noexcept_function_pointer_template(
                       value, conversion_explicit_noexcept_function) == 224 &&
                   choose_noexcept_function_pointer(
                       conversion_explicit_throwing_function) == 221 &&
                   choose_noexcept_function_pointer_template(
                       value, conversion_explicit_throwing_function) == 223 &&
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
                   choose_member_function_pointer_noexcept(
                       0, root_noexcept_member_function) == 205 &&
                   choose_member_function_pointer_noexcept(
                       0, root_throwing_member_function) == 206 &&
                   choose_member_function_pointer_noexcept_non_template(
                       root_noexcept_member_function) == 207 &&
                   choose_member_function_pointer_noexcept_non_template(
                       root_throwing_member_function) == 208 &&
                   choose_base_over_void(leaf_pointer, leaf_pointer) == 83 &&
                   choose_nearer_base_value(
                       value_leaf_pointer, value_leaf) == 92 &&
                   choose_nearer_base_value_non_template(value_leaf) == 94 &&
                   choose_nearer_base_value_non_template(
                       make_conversion_value_leaf()) == 94 &&
                   root_copy.root_value == 41 &&
                   cleanup_root_value == 53 &&
                   virtual_cleanup_count_before == cleanup_count_before + 1
               ? 0
               : 1;
}
