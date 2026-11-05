struct FunctionTemplateStructuralTarget {
    int value;
};

struct FunctionTemplateStructuralPair {
    unsigned short high;
    unsigned char low;
};

union FunctionTemplateStructuralUnion {
    int value;
    unsigned char tag;
};

union FunctionTemplateStructuralNestedUnion {
    int value;
    unsigned char tag;
};

union FunctionTemplateStructuralFloatingUnion {
    float single;
    double wide;
    long double extended;
};

union FunctionTemplateStructuralArrayUnion {
    int values[2];
    int scalar;
};

union FunctionTemplateStructuralMatrixArrayUnion {
    int values[2][2];
    int scalar;
};

union FunctionTemplateStructuralTensorArrayUnion {
    int values[2][2][2];
    int scalar;
};

union FunctionTemplateStructuralQuadArrayUnion {
    int values[2][2][2][2];
    int scalar;
};

union FunctionTemplateStructuralQuintArrayUnion {
    int values[2][2][2][2][2];
    int scalar;
};

#define FUNCTION_TEMPLATE_STRUCTURAL_QUINT_ARRAY_VALUE \
    FunctionTemplateStructuralQuintArrayUnion{ \
        .values = { \
            { \
                {{{1, 2}, {3, 4}}, {{5, 6}, {7, 8}}}, \
                {{{9, 10}, {11, 12}}, {{13, 14}, {15, 16}}}, \
            }, \
            { \
                {{{17, 18}, {19, 20}}, {{21, 22}, {23, 24}}}, \
                {{{25, 26}, {27, 28}}, {{29, 30}, {31, 32}}}, \
            }, \
        }}

struct FunctionTemplateStructuralPointerUnionPayload {
    const int* pointer;
    int bias;
};

union FunctionTemplateStructuralPointerUnion {
    FunctionTemplateStructuralPointerUnionPayload object;
    int scalar;
};

union FunctionTemplateStructuralDirectPointerUnion {
    const int* pointer;
    int scalar;
};

inline constexpr int function_template_structural_union_pointer_target = 23;

struct FunctionTemplateStructuralPointerFactoryValue {
    FunctionTemplateStructuralPointerUnion payload;
};

template<FunctionTemplateStructuralPointerFactoryValue Value>
struct FunctionTemplateStructuralPointerFactoryValueTag {};

constexpr FunctionTemplateStructuralPointerFactoryValue
function_template_make_structural_pointer_union_value(
    const int* pointer, int bias) {
    return {.payload = {.object = {pointer, bias}}};
}

template<int Adjustment>
constexpr FunctionTemplateStructuralPointerFactoryValue
function_template_make_structural_pointer_union_template_value(
    const int* pointer, int bias) {
    return {.payload = {.object = {pointer, bias + Adjustment}}};
}

template<typename PointerElement>
constexpr FunctionTemplateStructuralPointerFactoryValue
function_template_make_structural_pointer_union_deduced_value(
    PointerElement* pointer, int bias) {
    return {.payload = {.object = {pointer, bias}}};
}

struct FunctionTemplateStructuralPointerArray {
    const int* values[2];
    int bias;
};

struct FunctionTemplateStructuralLongDoubleMember {
    long double value;
    int bias;
};

struct FunctionTemplateStructuralLongDoubleArray {
    long double values[2];
    int bias;
};

struct FunctionTemplateStructuralDefaultMember {
    int value;
    int bias = 6;
};

struct FunctionTemplateStructuralConstexprConstructor {
    int value;

    constexpr explicit FunctionTemplateStructuralConstexprConstructor(
        int input) : value(input + 1) {}

    constexpr ~FunctionTemplateStructuralConstexprConstructor() {}
};

inline constexpr int function_template_structural_pointer_array_first = 13;
inline constexpr int function_template_structural_pointer_array_second = 29;

struct FunctionTemplateStructuralUnionContainer {
    unsigned char prefix;
    FunctionTemplateStructuralNestedUnion payload;
    unsigned short suffix;
};

struct FunctionTemplateStructuralUnionArray {
    FunctionTemplateStructuralNestedUnion payloads[2];
    unsigned char suffix;
};

struct FunctionTemplateStructuralUnionMatrix {
    FunctionTemplateStructuralNestedUnion payloads[2][2];
    unsigned char suffix;
};

struct FunctionTemplateStructuralUnionTensor {
    FunctionTemplateStructuralNestedUnion payloads[2][2][2];
    unsigned char suffix;
};

struct FunctionTemplateStructuralUnionQuadArray {
    FunctionTemplateStructuralNestedUnion payloads[2][2][2][2];
    unsigned char suffix;
};

struct FunctionTemplateStructuralUnionFiveDimensionalArray {
    FunctionTemplateStructuralNestedUnion payloads[2][1][1][1][2];
    unsigned char suffix;
};

#define FUNCTION_TEMPLATE_STRUCTURAL_FIVE_D_UNION_VALUE( \
    FIRST, FIRST_TAG, SECOND_TAG, SECOND_VALUE, SUFFIX) \
    FunctionTemplateStructuralUnionFiveDimensionalArray { \
        .payloads = { \
            {{{{{.value = FIRST}, {.tag = FIRST_TAG}}}}}, \
            {{{{{.tag = SECOND_TAG}, {.value = SECOND_VALUE}}}}}, \
        }, \
        .suffix = SUFFIX, \
    }

constexpr FunctionTemplateStructuralUnionFiveDimensionalArray
function_template_make_structural_five_d_union_value(
    int first, unsigned char first_tag, unsigned char second_tag,
    int second_value, unsigned char suffix) {
    return FUNCTION_TEMPLATE_STRUCTURAL_FIVE_D_UNION_VALUE(
        first, first_tag, second_tag, second_value, suffix);
}

template<int Adjustment>
constexpr FunctionTemplateStructuralUnionFiveDimensionalArray
function_template_make_structural_five_d_union_template_value(
    int first, unsigned char first_tag, unsigned char second_tag,
    int second_value, unsigned char suffix) {
    return FUNCTION_TEMPLATE_STRUCTURAL_FIVE_D_UNION_VALUE(
        first + Adjustment, first_tag, second_tag,
        second_value + Adjustment, suffix);
}

template<typename First>
constexpr FunctionTemplateStructuralUnionFiveDimensionalArray
function_template_make_structural_five_d_union_deduced_value(
    First first, unsigned char first_tag, unsigned char second_tag,
    int second_value, unsigned char suffix) {
    return FUNCTION_TEMPLATE_STRUCTURAL_FIVE_D_UNION_VALUE(
        static_cast<int>(first) + 2, first_tag, second_tag,
        second_value + 2, suffix);
}

struct FunctionTemplateStructuralUnionPayload {
    int code;
    int extra;
};

union FunctionTemplateStructuralMixedUnion {
    int scalar;
    FunctionTemplateStructuralUnionPayload object;
};

struct FunctionTemplateStructuralMixedUnionContainer {
    FunctionTemplateStructuralMixedUnion payload;
    unsigned short suffix;
};

struct FunctionTemplateStructuralBase {
    unsigned short prefix[2];
};

struct FunctionTemplateStructuralEmptyBase {};

struct FunctionTemplateStructuralEmptyMember {};

struct FunctionTemplateStructuralTagBase {
    unsigned char tag;
};

struct FunctionTemplateStructuralDerived : FunctionTemplateStructuralEmptyBase,
                                            FunctionTemplateStructuralBase,
                                            FunctionTemplateStructuralTagBase {
    FunctionTemplateStructuralEmptyMember empty_member;
    FunctionTemplateStructuralEmptyMember empty[2];
    unsigned char suffix;
    FunctionTemplateStructuralEmptyMember trailing_empty_member;
    FunctionTemplateStructuralEmptyMember trailing_empty[2];
};

inline constexpr FunctionTemplateStructuralTarget
    function_template_structural_target_one{37};
inline constexpr FunctionTemplateStructuralTarget
    function_template_structural_target_two{41};

struct FunctionTemplateStructuralReference {
    const FunctionTemplateStructuralTarget& target;
};

template<FunctionTemplateStructuralTarget Value>
struct FunctionTemplateStructuralValueTag {};

template<FunctionTemplateStructuralReference Value>
struct FunctionTemplateStructuralReferenceTag {};

template<FunctionTemplateStructuralPair Value>
struct FunctionTemplateStructuralPairTag {};

template<FunctionTemplateStructuralUnion Value>
struct FunctionTemplateStructuralUnionTag {};

template<FunctionTemplateStructuralFloatingUnion Value>
struct FunctionTemplateStructuralFloatingUnionTag {};

template<FunctionTemplateStructuralArrayUnion Value>
struct FunctionTemplateStructuralArrayUnionTag {};

template<FunctionTemplateStructuralMatrixArrayUnion Value>
struct FunctionTemplateStructuralMatrixArrayUnionTag {};

template<FunctionTemplateStructuralTensorArrayUnion Value>
struct FunctionTemplateStructuralTensorArrayUnionTag {};

template<FunctionTemplateStructuralQuadArrayUnion Value>
struct FunctionTemplateStructuralQuadArrayUnionTag {};

template<FunctionTemplateStructuralQuintArrayUnion Value>
struct FunctionTemplateStructuralQuintArrayUnionTag {};

template<FunctionTemplateStructuralPointerUnion Value>
struct FunctionTemplateStructuralPointerUnionTag {};

template<FunctionTemplateStructuralDirectPointerUnion Value>
struct FunctionTemplateStructuralDirectPointerUnionTag {};

template<FunctionTemplateStructuralPointerArray Value>
struct FunctionTemplateStructuralPointerArrayTag {};

template<FunctionTemplateStructuralLongDoubleMember Value>
struct FunctionTemplateStructuralLongDoubleMemberTag {};

template<FunctionTemplateStructuralLongDoubleArray Value>
struct FunctionTemplateStructuralLongDoubleArrayTag {};

template<FunctionTemplateStructuralDefaultMember Value>
struct FunctionTemplateStructuralDefaultMemberTag {};

template<FunctionTemplateStructuralConstexprConstructor Value>
struct FunctionTemplateStructuralConstexprConstructorTag {};

template<FunctionTemplateStructuralUnionContainer Value>
struct FunctionTemplateStructuralUnionContainerTag {};

template<FunctionTemplateStructuralUnionArray Value>
struct FunctionTemplateStructuralUnionArrayTag {};

template<FunctionTemplateStructuralUnionMatrix Value>
struct FunctionTemplateStructuralUnionMatrixTag {};

template<FunctionTemplateStructuralUnionTensor Value>
struct FunctionTemplateStructuralUnionTensorTag {};

template<FunctionTemplateStructuralUnionQuadArray Value>
struct FunctionTemplateStructuralUnionQuadArrayTag {};

template<FunctionTemplateStructuralUnionFiveDimensionalArray Value>
struct FunctionTemplateStructuralUnionFiveDimensionalArrayTag {};

template<FunctionTemplateStructuralMixedUnionContainer Value>
struct FunctionTemplateStructuralMixedUnionTag {};

template<FunctionTemplateStructuralDerived Value>
struct FunctionTemplateStructuralDerivedTag {};

template<FunctionTemplateStructuralTarget Value>
constexpr int function_template_structural_value_explicit() {
    return Value.value;
}

template<FunctionTemplateStructuralTarget Value>
constexpr int function_template_structural_value_deduced(
    FunctionTemplateStructuralValueTag<Value>) {
    return Value.value;
}

template<FunctionTemplateStructuralReference Value>
constexpr int function_template_structural_reference_explicit() {
    return Value.target.value;
}

template<FunctionTemplateStructuralReference Value>
constexpr int function_template_structural_reference_deduced(
    FunctionTemplateStructuralReferenceTag<Value>) {
    return Value.target.value;
}

template<FunctionTemplateStructuralPair Value>
constexpr int function_template_structural_pair_explicit() {
    return Value.high * 100 + Value.low;
}

template<FunctionTemplateStructuralPair Value>
constexpr int function_template_structural_pair_deduced(
    FunctionTemplateStructuralPairTag<Value>) {
    return Value.high * 100 + Value.low;
}

template<FunctionTemplateStructuralUnion Value>
constexpr int function_template_structural_union_value_explicit() {
    return Value.value;
}

template<FunctionTemplateStructuralUnion Value>
constexpr int function_template_structural_union_value_deduced(
    FunctionTemplateStructuralUnionTag<Value>) {
    return Value.value;
}

template<FunctionTemplateStructuralUnion Value>
constexpr int function_template_structural_union_tag_explicit() {
    return Value.tag;
}

template<FunctionTemplateStructuralUnion Value>
constexpr int function_template_structural_union_tag_deduced(
    FunctionTemplateStructuralUnionTag<Value>) {
    return Value.tag;
}

template<FunctionTemplateStructuralUnionContainer Value>
constexpr int function_template_structural_nested_union_value_explicit() {
    return Value.prefix * 10000 + Value.payload.value * 10 + Value.suffix;
}

template<FunctionTemplateStructuralUnionContainer Value>
constexpr int function_template_structural_nested_union_value_deduced(
    FunctionTemplateStructuralUnionContainerTag<Value>) {
    return Value.prefix * 10000 + Value.payload.value * 10 + Value.suffix;
}

template<FunctionTemplateStructuralUnionContainer Value>
constexpr int function_template_structural_nested_union_tag_explicit() {
    return Value.prefix * 10000 + Value.payload.tag * 10 + Value.suffix;
}

template<FunctionTemplateStructuralUnionContainer Value>
constexpr int function_template_structural_nested_union_tag_deduced(
    FunctionTemplateStructuralUnionContainerTag<Value>) {
    return Value.prefix * 10000 + Value.payload.tag * 10 + Value.suffix;
}

template<FunctionTemplateStructuralUnionArray Value>
constexpr int function_template_structural_union_array_value_explicit() {
    return Value.payloads[1].value * 100 +
           Value.payloads[0].value * 10 + Value.suffix;
}

template<FunctionTemplateStructuralUnionArray Value>
constexpr int function_template_structural_union_array_value_deduced(
    FunctionTemplateStructuralUnionArrayTag<Value>) {
    return Value.payloads[1].value * 100 +
           Value.payloads[0].value * 10 + Value.suffix;
}

template<FunctionTemplateStructuralUnionArray Value>
constexpr int function_template_structural_union_array_tag_explicit() {
    return Value.payloads[1].tag * 100 +
           Value.payloads[0].tag * 10 + Value.suffix;
}

template<FunctionTemplateStructuralUnionArray Value>
constexpr int function_template_structural_union_array_tag_deduced(
    FunctionTemplateStructuralUnionArrayTag<Value>) {
    return Value.payloads[1].tag * 100 +
           Value.payloads[0].tag * 10 + Value.suffix;
}

template<FunctionTemplateStructuralUnionMatrix Value>
constexpr int function_template_structural_union_matrix_value_explicit() {
    return Value.payloads[1][0].value * 100 +
           Value.payloads[0][1].value * 10 + Value.suffix;
}

template<FunctionTemplateStructuralUnionMatrix Value>
constexpr int function_template_structural_union_matrix_value_deduced(
    FunctionTemplateStructuralUnionMatrixTag<Value>) {
    return Value.payloads[1][0].value * 100 +
           Value.payloads[0][1].value * 10 + Value.suffix;
}

template<FunctionTemplateStructuralUnionMatrix Value>
constexpr int function_template_structural_union_matrix_tag_explicit() {
    return Value.payloads[1][0].tag * 100 +
           Value.payloads[0][1].tag * 10 + Value.suffix;
}

template<FunctionTemplateStructuralUnionMatrix Value>
constexpr int function_template_structural_union_matrix_tag_deduced(
    FunctionTemplateStructuralUnionMatrixTag<Value>) {
    return Value.payloads[1][0].tag * 100 +
           Value.payloads[0][1].tag * 10 + Value.suffix;
}

template<FunctionTemplateStructuralUnionTensor Value>
constexpr int function_template_structural_union_tensor_explicit() {
    return Value.payloads[1][0][1].value * 100 +
           Value.payloads[0][1][0].tag * 10 + Value.suffix;
}

template<FunctionTemplateStructuralUnionTensor Value>
constexpr int function_template_structural_union_tensor_deduced(
    FunctionTemplateStructuralUnionTensorTag<Value>) {
    return Value.payloads[1][0][1].value * 100 +
           Value.payloads[0][1][0].tag * 10 + Value.suffix;
}

template<FunctionTemplateStructuralUnionQuadArray Value>
constexpr int function_template_structural_union_quad_array_explicit() {
    return Value.payloads[1][0][1][0].value * 1000 +
           Value.payloads[0][1][0][1].value * 10 + Value.suffix;
}

template<FunctionTemplateStructuralUnionQuadArray Value>
constexpr int function_template_structural_union_quad_array_deduced(
    FunctionTemplateStructuralUnionQuadArrayTag<Value>) {
    return Value.payloads[1][0][1][0].value * 1000 +
           Value.payloads[0][1][0][1].value * 10 + Value.suffix;
}

template<FunctionTemplateStructuralUnionFiveDimensionalArray Value>
constexpr int function_template_structural_union_five_dimensional_array_explicit() {
    return Value.payloads[1][0][0][0][1].value * 100 +
           Value.payloads[0][0][0][0][1].tag * 10 + Value.suffix;
}

template<FunctionTemplateStructuralUnionFiveDimensionalArray Value>
constexpr int function_template_structural_union_five_dimensional_array_deduced(
    FunctionTemplateStructuralUnionFiveDimensionalArrayTag<Value>) {
    return Value.payloads[1][0][0][0][1].value * 100 +
           Value.payloads[0][0][0][0][1].tag * 10 + Value.suffix;
}

template<FunctionTemplateStructuralFloatingUnion Value>
constexpr int function_template_structural_union_float_explicit() {
    return Value.single == 1.5f ? 15 : 0;
}

template<FunctionTemplateStructuralFloatingUnion Value>
constexpr int function_template_structural_union_double_deduced(
    FunctionTemplateStructuralFloatingUnionTag<Value>) {
    return Value.wide == 2.25 ? 22 : 0;
}

template<FunctionTemplateStructuralFloatingUnion Value>
constexpr int function_template_structural_union_long_double_explicit() {
    return Value.extended == 1.25L ? 125 : 0;
}

template<FunctionTemplateStructuralFloatingUnion Value>
constexpr int function_template_structural_union_long_double_deduced(
    FunctionTemplateStructuralFloatingUnionTag<Value>) {
    return Value.extended == 2.5L ? 250 : 0;
}

template<FunctionTemplateStructuralPointerUnion Value>
constexpr int
function_template_structural_union_nested_pointer_explicit() {
    return *Value.object.pointer + Value.object.bias;
}

template<FunctionTemplateStructuralPointerUnion Value>
constexpr int function_template_structural_union_nested_pointer_deduced(
    FunctionTemplateStructuralPointerUnionTag<Value>) {
    return *Value.object.pointer + Value.object.bias;
}

template<FunctionTemplateStructuralPointerFactoryValue Value>
constexpr int function_template_structural_pointer_factory_explicit() {
    return *Value.payload.object.pointer + Value.payload.object.bias;
}

template<FunctionTemplateStructuralPointerFactoryValue Value>
constexpr int function_template_structural_pointer_factory_deduced(
    FunctionTemplateStructuralPointerFactoryValueTag<Value>) {
    return *Value.payload.object.pointer + Value.payload.object.bias;
}

template<FunctionTemplateStructuralDirectPointerUnion Value>
constexpr int
function_template_structural_union_direct_pointer_explicit() {
    return *Value.pointer;
}

template<FunctionTemplateStructuralDirectPointerUnion Value>
constexpr int function_template_structural_union_direct_pointer_deduced(
    FunctionTemplateStructuralDirectPointerUnionTag<Value>) {
    return *Value.pointer;
}

template<FunctionTemplateStructuralPointerArray Value>
constexpr int function_template_structural_pointer_array_explicit() {
    return *Value.values[0] * 100 + *Value.values[1] * 10 + Value.bias;
}

template<FunctionTemplateStructuralPointerArray Value>
constexpr int function_template_structural_pointer_array_deduced(
    FunctionTemplateStructuralPointerArrayTag<Value>) {
    return *Value.values[0] * 100 + *Value.values[1] * 10 + Value.bias;
}

template<FunctionTemplateStructuralLongDoubleMember Value>
constexpr int function_template_structural_long_double_member_explicit() {
    return Value.value == 1.25L ? Value.bias : 0;
}

template<FunctionTemplateStructuralLongDoubleMember Value>
constexpr int function_template_structural_long_double_member_deduced(
    FunctionTemplateStructuralLongDoubleMemberTag<Value>) {
    return Value.value == 2.5L ? Value.bias : 0;
}

template<FunctionTemplateStructuralLongDoubleArray Value>
constexpr int function_template_structural_long_double_array_explicit() {
    return Value.values[0] == 1.25L && Value.values[1] == 2.5L
        ? Value.bias : 0;
}

template<FunctionTemplateStructuralLongDoubleArray Value>
constexpr int function_template_structural_long_double_array_deduced(
    FunctionTemplateStructuralLongDoubleArrayTag<Value>) {
    return Value.values[0] == 2.5L && Value.values[1] == 1.25L
        ? Value.bias : 0;
}

template<FunctionTemplateStructuralDefaultMember Value>
constexpr int function_template_structural_default_member_explicit() {
    return Value.value * 10 + Value.bias;
}

template<FunctionTemplateStructuralDefaultMember Value>
constexpr int function_template_structural_default_member_deduced(
    FunctionTemplateStructuralDefaultMemberTag<Value>) {
    return Value.value * 10 + Value.bias;
}

template<FunctionTemplateStructuralConstexprConstructor Value>
constexpr int function_template_structural_constexpr_constructor_explicit() {
    return Value.value;
}

template<FunctionTemplateStructuralConstexprConstructor Value>
constexpr int function_template_structural_constexpr_constructor_deduced(
    FunctionTemplateStructuralConstexprConstructorTag<Value>) {
    return Value.value;
}

template<FunctionTemplateStructuralDirectPointerUnion Value>
constexpr int function_template_structural_union_null_pointer_explicit() {
    return Value.pointer == nullptr ? 1 : 0;
}

template<FunctionTemplateStructuralArrayUnion Value>
constexpr int function_template_structural_union_array_explicit() {
    return Value.values[0] * 10 + Value.values[1];
}

template<FunctionTemplateStructuralArrayUnion Value>
constexpr int function_template_structural_union_array_deduced(
    FunctionTemplateStructuralArrayUnionTag<Value>) {
    return Value.values[0] * 10 + Value.values[1];
}

template<FunctionTemplateStructuralArrayUnion Value>
constexpr int function_template_structural_union_scalar_alternative_explicit() {
    return Value.scalar * 10 + 2;
}

template<FunctionTemplateStructuralMatrixArrayUnion Value>
constexpr int
function_template_structural_union_matrix_array_explicit() {
    return Value.values[1][0] * 1000 + Value.values[0][1] * 100 +
           Value.values[1][1] * 10 + Value.values[0][0];
}

template<FunctionTemplateStructuralMatrixArrayUnion Value>
constexpr int function_template_structural_union_matrix_array_deduced(
    FunctionTemplateStructuralMatrixArrayUnionTag<Value>) {
    return Value.values[1][0] * 1000 + Value.values[0][1] * 100 +
           Value.values[1][1] * 10 + Value.values[0][0];
}

template<FunctionTemplateStructuralTensorArrayUnion Value>
constexpr int function_template_structural_union_tensor_array_explicit() {
    return Value.values[0][0][0] * 10000000 +
           Value.values[0][0][1] * 1000000 +
           Value.values[0][1][0] * 100000 +
           Value.values[0][1][1] * 10000 +
           Value.values[1][0][0] * 1000 +
           Value.values[1][0][1] * 100 +
           Value.values[1][1][0] * 10 + Value.values[1][1][1];
}

template<FunctionTemplateStructuralTensorArrayUnion Value>
constexpr int function_template_structural_union_tensor_array_deduced(
    FunctionTemplateStructuralTensorArrayUnionTag<Value>) {
    return Value.values[0][0][0] * 10000000 +
           Value.values[0][0][1] * 1000000 +
           Value.values[0][1][0] * 100000 +
           Value.values[0][1][1] * 10000 +
           Value.values[1][0][0] * 1000 +
           Value.values[1][0][1] * 100 +
           Value.values[1][1][0] * 10 + Value.values[1][1][1];
}

template<FunctionTemplateStructuralQuadArrayUnion Value>
constexpr int function_template_structural_union_quad_array_alternative_explicit() {
    return Value.values[1][0][1][0] * 1000 +
           Value.values[0][1][0][1] * 10 + Value.values[1][1][1][1];
}

template<FunctionTemplateStructuralQuadArrayUnion Value>
constexpr int function_template_structural_union_quad_array_alternative_deduced(
    FunctionTemplateStructuralQuadArrayUnionTag<Value>) {
    return Value.values[1][0][1][0] * 1000 +
           Value.values[0][1][0][1] * 10 + Value.values[1][1][1][1];
}

template<FunctionTemplateStructuralQuintArrayUnion Value>
constexpr int function_template_structural_union_quint_array_explicit() {
    return Value.values[1][0][1][0][1] * 1000 +
           Value.values[0][1][0][1][0] * 10 +
           Value.values[1][1][1][1][1];
}

template<FunctionTemplateStructuralQuintArrayUnion Value>
constexpr int function_template_structural_union_quint_array_deduced(
    FunctionTemplateStructuralQuintArrayUnionTag<Value>) {
    return Value.values[1][0][1][0][1] * 1000 +
           Value.values[0][1][0][1][0] * 10 +
           Value.values[1][1][1][1][1];
}

template<FunctionTemplateStructuralMixedUnionContainer Value>
constexpr int function_template_structural_mixed_union_object_explicit() {
    return Value.payload.object.code * 100 +
           Value.payload.object.extra * 10 + Value.suffix;
}

template<FunctionTemplateStructuralMixedUnionContainer Value>
constexpr int function_template_structural_mixed_union_object_deduced(
    FunctionTemplateStructuralMixedUnionTag<Value>) {
    return Value.payload.object.code * 100 +
           Value.payload.object.extra * 10 + Value.suffix;
}

template<FunctionTemplateStructuralMixedUnionContainer Value>
constexpr int function_template_structural_mixed_union_scalar_explicit() {
    return Value.payload.scalar * 10 + Value.suffix;
}

template<FunctionTemplateStructuralDerived Value>
constexpr int function_template_structural_derived_explicit() {
    return Value.tag * 10000 + Value.prefix[1] * 100 + Value.suffix;
}

template<FunctionTemplateStructuralDerived Value>
constexpr int function_template_structural_derived_deduced(
    FunctionTemplateStructuralDerivedTag<Value>) {
    return Value.tag * 10000 + Value.prefix[1] * 100 + Value.suffix;
}

static_assert(function_template_structural_value_explicit<
                  FunctionTemplateStructuralTarget{37}>() == 37);
static_assert(function_template_structural_value_deduced(
                  FunctionTemplateStructuralValueTag<
                      FunctionTemplateStructuralTarget{41}>{}) == 41);
static_assert(function_template_structural_value_explicit<
                  FunctionTemplateStructuralTarget(43)>() == 43);
static_assert(function_template_structural_value_deduced(
                  FunctionTemplateStructuralValueTag<
                      FunctionTemplateStructuralTarget(47)>{}) == 47);
static_assert(function_template_structural_constexpr_constructor_explicit<
                  FunctionTemplateStructuralConstexprConstructor(67)>() ==
              68);
static_assert(function_template_structural_constexpr_constructor_deduced(
                  FunctionTemplateStructuralConstexprConstructorTag<
                      FunctionTemplateStructuralConstexprConstructor(71)>{}) ==
              72);
static_assert(function_template_structural_value_explicit<
                  function_template_structural_target_one>() == 37);
static_assert(function_template_structural_value_deduced(
                  FunctionTemplateStructuralValueTag<
                      function_template_structural_target_two>{}) == 41);
static_assert(function_template_structural_reference_explicit<
                  FunctionTemplateStructuralReference{
                      function_template_structural_target_one}>() == 37);
static_assert(function_template_structural_reference_explicit<
                  FunctionTemplateStructuralReference{
                      function_template_structural_target_two}>() == 41);
static_assert(function_template_structural_reference_deduced(
                  FunctionTemplateStructuralReferenceTag<
                      FunctionTemplateStructuralReference{
                          function_template_structural_target_one}>{}) == 37);
static_assert(function_template_structural_reference_deduced(
                  FunctionTemplateStructuralReferenceTag<
                      FunctionTemplateStructuralReference{
                          function_template_structural_target_two}>{}) == 41);
static_assert(function_template_structural_pair_explicit<
                  FunctionTemplateStructuralPair{12, 5}>() == 1205);
static_assert(function_template_structural_pair_deduced(
                  FunctionTemplateStructuralPairTag<
                      FunctionTemplateStructuralPair{8, 3}>{}) == 803);
static_assert(function_template_structural_union_value_explicit<
                  FunctionTemplateStructuralUnion{.value = 17}>() == 17);
static_assert(function_template_structural_union_value_explicit<
                  FunctionTemplateStructuralUnion{17}>() == 17);
static_assert(function_template_structural_union_value_deduced(
                  FunctionTemplateStructuralUnionTag<
                      FunctionTemplateStructuralUnion{.value = 19}>{}) == 19);
static_assert(function_template_structural_union_tag_explicit<
                  FunctionTemplateStructuralUnion{.tag = 5}>() == 5);
static_assert(function_template_structural_union_tag_deduced(
                  FunctionTemplateStructuralUnionTag<
                      FunctionTemplateStructuralUnion{.tag = 7}>{}) == 7);
static_assert(function_template_structural_nested_union_value_explicit<
                  FunctionTemplateStructuralUnionContainer{
                      2, {.value = 19}, 3}>() == 20193);
static_assert(function_template_structural_nested_union_value_explicit<
                  FunctionTemplateStructuralUnionContainer{
                      2, {19}, 3}>() == 20193);
static_assert(function_template_structural_nested_union_value_deduced(
                  FunctionTemplateStructuralUnionContainerTag<
                      FunctionTemplateStructuralUnionContainer{
                          2, {19}, 4}>{}) == 20194);
static_assert(function_template_structural_nested_union_tag_explicit<
                  FunctionTemplateStructuralUnionContainer{
                      5, {.tag = 7}, 6}>() == 50076);
static_assert(function_template_structural_nested_union_tag_deduced(
                  FunctionTemplateStructuralUnionContainerTag<
                      FunctionTemplateStructuralUnionContainer{
                          5, {.tag = 8}, 9}>{}) == 50089);
static_assert(function_template_structural_union_array_value_explicit<
                  FunctionTemplateStructuralUnionArray{
                      {{.value = 2}, {.value = 3}}, 4}>() == 324);
static_assert(function_template_structural_union_array_value_explicit<
                  FunctionTemplateStructuralUnionArray{
                      {{2}, {3}}, 4}>() == 324);
static_assert(function_template_structural_union_array_value_deduced(
                  FunctionTemplateStructuralUnionArrayTag<
                      FunctionTemplateStructuralUnionArray{
                          {{2}, {.value = 5}}, 6}>{}) == 526);
static_assert(function_template_structural_union_array_tag_explicit<
                  FunctionTemplateStructuralUnionArray{
                      {{.tag = 7}, {.tag = 8}}, 9}>() == 879);
static_assert(function_template_structural_union_array_tag_deduced(
                  FunctionTemplateStructuralUnionArrayTag<
                      FunctionTemplateStructuralUnionArray{
                          {{.tag = 1}, {.tag = 4}}, 2}>{}) == 412);
static_assert(function_template_structural_union_matrix_value_explicit<
                  FunctionTemplateStructuralUnionMatrix{
                      {{{.value = 1}, {.value = 2}},
                       {{.value = 3}, {.value = 4}}}, 5}>() == 325);
static_assert(function_template_structural_union_matrix_value_explicit<
                  FunctionTemplateStructuralUnionMatrix{
                      {{{1}, {2}}, {{3}, {4}}}, 5}>() == 325);
static_assert(function_template_structural_union_matrix_value_deduced(
                  FunctionTemplateStructuralUnionMatrixTag<
                      FunctionTemplateStructuralUnionMatrix{
                          {{{1}, {.value = 5}},
                           {{.value = 6}, {7}}}, 8}>{}) == 658);
static_assert(function_template_structural_union_matrix_tag_explicit<
                  FunctionTemplateStructuralUnionMatrix{
                      {{{.tag = 1}, {.tag = 2}},
                       {{.tag = 3}, {.tag = 4}}}, 5}>() == 325);
static_assert(function_template_structural_union_matrix_tag_deduced(
                  FunctionTemplateStructuralUnionMatrixTag<
                      FunctionTemplateStructuralUnionMatrix{
                          {{{.tag = 5}, {.tag = 6}},
                           {{.tag = 7}, {.tag = 8}}}, 9}>{}) == 769);
static_assert(function_template_structural_union_tensor_explicit<
                  FunctionTemplateStructuralUnionTensor{
                      {{{{.value = 1}, {.tag = 2}},
                        {{.tag = 3}, {.tag = 4}}},
                       {{{.tag = 5}, {.value = 6}},
                         {{.tag = 7}, {.value = 8}}}}, 9}>() == 639);
static_assert(function_template_structural_union_tensor_deduced(
                  FunctionTemplateStructuralUnionTensorTag<
                      FunctionTemplateStructuralUnionTensor{
                          {{{{.value = 2}, {.tag = 3}},
                            {{.tag = 4}, {.tag = 5}}},
                           {{{.tag = 6}, {.value = 7}},
                           {{.tag = 8}, {.value = 9}}}}, 1}>{}) == 741);
static_assert(function_template_structural_union_quad_array_explicit<
                  FunctionTemplateStructuralUnionQuadArray{
                      .payloads = {
                          {
                              {
                                  {{.tag = 2}, {.value = 3}},
                                  {{.value = 4}, {.tag = 5}}},
                              {
                                  {{.tag = 6}, {.value = 7}},
                                  {{.value = 8}, {.tag = 9}}}},
                          {
                              {
                                  {{.tag = 10}, {.value = 11}},
                                  {{.value = 12}, {.tag = 13}}},
                              {
                                  {{.tag = 14}, {.value = 15}},
                                  {{.value = 16}, {.tag = 17}}}}},
                      .suffix = 9}>() == 12079);
static_assert(function_template_structural_union_quad_array_deduced(
                  FunctionTemplateStructuralUnionQuadArrayTag<
                      FunctionTemplateStructuralUnionQuadArray{
                          {
                              {
                                  {
                                      {{.tag = 18}, {.value = 19}},
                                      {{.value = 20}, {.tag = 21}}},
                                  {
                                      {{.tag = 22}, {.value = 23}},
                                      {{.value = 24}, {.tag = 25}}}},
                              {
                                  {
                                      {{.tag = 26}, {.value = 27}},
                                      {{.value = 28}, {.tag = 29}}},
                                  {
                                      {{.tag = 30}, {.value = 31}},
                                      {{.value = 32}, {.tag = 33}}}}},
                          1}>{}) == 28231);
static_assert(function_template_structural_union_five_dimensional_array_explicit<
                  function_template_make_structural_five_d_union_value(
                      3, 7, 4, 17, 9)>() == 1779);
static_assert(function_template_structural_union_five_dimensional_array_deduced(
                  FunctionTemplateStructuralUnionFiveDimensionalArrayTag<
                      function_template_make_structural_five_d_union_value(
                          5, 8, 6, 18, 1)>{}) == 1881);
static_assert(function_template_structural_union_five_dimensional_array_explicit<
                  function_template_make_structural_five_d_union_template_value<
                      2>(3, 7, 4, 17, 9)>() == 1979);
static_assert(function_template_structural_union_five_dimensional_array_deduced(
                  FunctionTemplateStructuralUnionFiveDimensionalArrayTag<
                      function_template_make_structural_five_d_union_template_value<
                          3>(5, 8, 6, 18, 1)>{}) == 2181);
static_assert(function_template_structural_union_five_dimensional_array_explicit<
                  function_template_make_structural_five_d_union_deduced_value(
                      4, 6, 5, 12, 8)>() == 1468);
static_assert(function_template_structural_union_five_dimensional_array_deduced(
                  FunctionTemplateStructuralUnionFiveDimensionalArrayTag<
                      function_template_make_structural_five_d_union_deduced_value(
                          static_cast<unsigned char>(2), 9, 4, 14, 3)>{}) ==
              1693);
static_assert(function_template_structural_union_float_explicit<
                  FunctionTemplateStructuralFloatingUnion{
                      .single = 1.5f}>() == 15);
static_assert(function_template_structural_union_double_deduced(
                  FunctionTemplateStructuralFloatingUnionTag<
                      FunctionTemplateStructuralFloatingUnion{
                          .wide = 2.25}>{}) == 22);
static_assert(function_template_structural_union_long_double_explicit<
                  FunctionTemplateStructuralFloatingUnion{
                      .extended = 1.25L}>() == 125);
static_assert(function_template_structural_union_long_double_deduced(
                  FunctionTemplateStructuralFloatingUnionTag<
                      FunctionTemplateStructuralFloatingUnion{
                          .extended = 2.5L}>{}) == 250);
static_assert(function_template_structural_union_null_pointer_explicit<
                  FunctionTemplateStructuralDirectPointerUnion{
                      .pointer = nullptr}>() == 1);
static_assert(function_template_structural_union_nested_pointer_explicit<
                  FunctionTemplateStructuralPointerUnion{
                      .object = {&function_template_structural_union_pointer_target,
                                 4}}>() == 27);
static_assert(function_template_structural_union_nested_pointer_deduced(
                  FunctionTemplateStructuralPointerUnionTag<
                      FunctionTemplateStructuralPointerUnion{
                          .object = {
                              &function_template_structural_union_pointer_target,
                              5}}>{}) == 28);
static_assert(function_template_structural_pointer_factory_explicit<
                  function_template_make_structural_pointer_union_value(
                      &function_template_structural_union_pointer_target,
                      8)>() == 31);
static_assert(function_template_structural_pointer_factory_explicit<
                  function_template_make_structural_pointer_union_template_value<
                      2>(
                      &function_template_structural_union_pointer_target,
                      4)>() == 29);
static_assert(function_template_structural_pointer_factory_deduced(
                  FunctionTemplateStructuralPointerFactoryValueTag<
                      function_template_make_structural_pointer_union_deduced_value(
                          &function_template_structural_union_pointer_target,
                          7)>{}) == 30);
static_assert(function_template_structural_union_direct_pointer_explicit<
                  FunctionTemplateStructuralDirectPointerUnion{
                      .pointer =
                          &function_template_structural_union_pointer_target}>() ==
              23);
static_assert(function_template_structural_union_direct_pointer_deduced(
                  FunctionTemplateStructuralDirectPointerUnionTag<
                      FunctionTemplateStructuralDirectPointerUnion{
                          .pointer =
                              &function_template_structural_union_pointer_target}>{}) ==
              23);
static_assert(function_template_structural_pointer_array_explicit<
                  FunctionTemplateStructuralPointerArray{
                      .values = {
                          &function_template_structural_pointer_array_first,
                          &function_template_structural_pointer_array_second},
                      .bias = 7}>() == 1597);
static_assert(function_template_structural_pointer_array_deduced(
                  FunctionTemplateStructuralPointerArrayTag<
                      FunctionTemplateStructuralPointerArray{
                          {&function_template_structural_pointer_array_second,
                           &function_template_structural_pointer_array_first},
                          5}>{}) == 3035);
static_assert(function_template_structural_long_double_member_explicit<
                  FunctionTemplateStructuralLongDoubleMember{1.25L, 7}>() ==
              7);
static_assert(function_template_structural_long_double_member_deduced(
                  FunctionTemplateStructuralLongDoubleMemberTag<
                      FunctionTemplateStructuralLongDoubleMember{2.5L, 5}>{}) ==
              5);
static_assert(function_template_structural_long_double_array_explicit<
                  FunctionTemplateStructuralLongDoubleArray{
                      .values = {1.25L, 2.5L}, .bias = 7}>() == 7);
static_assert(function_template_structural_long_double_array_deduced(
                  FunctionTemplateStructuralLongDoubleArrayTag<
                      FunctionTemplateStructuralLongDoubleArray{
                          {2.5L, 1.25L}, 5}>{}) == 5);
static_assert(function_template_structural_default_member_explicit<
                  FunctionTemplateStructuralDefaultMember{.value = 4}>() ==
              46);
static_assert(function_template_structural_default_member_deduced(
                  FunctionTemplateStructuralDefaultMemberTag<
                      FunctionTemplateStructuralDefaultMember{5}>{}) == 56);
static_assert(function_template_structural_union_array_explicit<
                  FunctionTemplateStructuralArrayUnion{
                      .values = {4, 2}}>() == 42);
static_assert(function_template_structural_union_array_deduced(
                  FunctionTemplateStructuralArrayUnionTag<
                      FunctionTemplateStructuralArrayUnion{
                          {5, 3}}>{}) == 53);
static_assert(function_template_structural_union_scalar_alternative_explicit<
                  FunctionTemplateStructuralArrayUnion{
                      .scalar = 4}>() == 42);
static_assert(function_template_structural_union_matrix_array_explicit<
                  FunctionTemplateStructuralMatrixArrayUnion{
                      .values = {{1, 2}, {3, 4}}}>() == 3241);
static_assert(function_template_structural_union_matrix_array_deduced(
                  FunctionTemplateStructuralMatrixArrayUnionTag<
                      FunctionTemplateStructuralMatrixArrayUnion{
                          {{5, 6}, {7, 8}}}>{}) == 7685);
static_assert(function_template_structural_union_tensor_array_explicit<
                  FunctionTemplateStructuralTensorArrayUnion{
                      .values = {{{1, 2}, {3, 4}}, {{5, 6}, {7, 8}}}}>() ==
              12345678);
static_assert(function_template_structural_union_tensor_array_deduced(
                  FunctionTemplateStructuralTensorArrayUnionTag<
                      FunctionTemplateStructuralTensorArrayUnion{
                          {{{8, 7}, {6, 5}}, {{4, 3}, {2, 1}}}}>{}) ==
              87654321);
static_assert(function_template_structural_union_quad_array_alternative_explicit<
                  FunctionTemplateStructuralQuadArrayUnion{
                      .values = {{{{1, 2}, {3, 4}},
                                  {{5, 6}, {7, 8}}},
                                 {{{9, 10}, {11, 12}},
                                  {{13, 14}, {15, 16}}}}}>() == 11076);
static_assert(function_template_structural_union_quad_array_alternative_deduced(
                  FunctionTemplateStructuralQuadArrayUnionTag<
                      FunctionTemplateStructuralQuadArrayUnion{
                          {{{{16, 15}, {14, 13}},
                            {{12, 11}, {10, 9}}},
                           {{{8, 7}, {6, 5}},
                            {{4, 3}, {2, 1}}}}}>{}) == 6111);
static_assert(function_template_structural_mixed_union_object_explicit<
                  FunctionTemplateStructuralMixedUnionContainer{
                      {.object = {4, 7}}, 2}>() == 472);
static_assert(function_template_structural_mixed_union_object_explicit<
                  FunctionTemplateStructuralMixedUnionContainer{
                      {.object = {.code = 4, .extra = 7}}, 2}>() == 472);
static_assert(function_template_structural_mixed_union_object_deduced(
                  FunctionTemplateStructuralMixedUnionTag<
                      FunctionTemplateStructuralMixedUnionContainer{
                          {.object = {5, 6}}, 3}>{}) == 563);
static_assert(function_template_structural_mixed_union_scalar_explicit<
                  FunctionTemplateStructuralMixedUnionContainer{
                      {.scalar = 4}, 2}>() == 42);
static_assert(function_template_structural_derived_explicit<
                  FunctionTemplateStructuralDerived{
                      {}, {{11, 23}}, {2}, {}, {{}, {}}, 7, {},
                      {{}, {}}}>() == 22307);
static_assert(function_template_structural_derived_deduced(
                  FunctionTemplateStructuralDerivedTag<
                      FunctionTemplateStructuralDerived{
                          {}, {{4, 9}}, {3}, {}, {{}, {}}, 4, {},
                          {{}, {}}}>{}) == 30904);
static_assert(function_template_structural_union_quint_array_explicit<
                  FUNCTION_TEMPLATE_STRUCTURAL_QUINT_ARRAY_VALUE>() ==
              22142);
static_assert(function_template_structural_union_quint_array_deduced(
                  FunctionTemplateStructuralQuintArrayUnionTag<
                      FUNCTION_TEMPLATE_STRUCTURAL_QUINT_ARRAY_VALUE>{}) ==
              22142);
int main() {
    if (function_template_structural_value_explicit<
            FunctionTemplateStructuralTarget{37}>() != 37 ||
        function_template_structural_value_deduced(
            FunctionTemplateStructuralValueTag<
                FunctionTemplateStructuralTarget{41}>{}) != 41 ||
        function_template_structural_value_explicit<
            FunctionTemplateStructuralTarget(43)>() != 43 ||
        function_template_structural_value_deduced(
            FunctionTemplateStructuralValueTag<
                FunctionTemplateStructuralTarget(47)>{}) != 47 ||
        function_template_structural_constexpr_constructor_explicit<
            FunctionTemplateStructuralConstexprConstructor(67)>() != 68 ||
        function_template_structural_constexpr_constructor_deduced(
            FunctionTemplateStructuralConstexprConstructorTag<
                FunctionTemplateStructuralConstexprConstructor(71)>{}) != 72 ||
        function_template_structural_value_explicit<
            function_template_structural_target_one>() != 37 ||
        function_template_structural_value_deduced(
            FunctionTemplateStructuralValueTag<
                function_template_structural_target_two>{}) != 41 ||
        function_template_structural_reference_explicit<
            FunctionTemplateStructuralReference{
                function_template_structural_target_one}>() != 37 ||
        function_template_structural_reference_explicit<
            FunctionTemplateStructuralReference{
                function_template_structural_target_two}>() != 41 ||
        function_template_structural_reference_deduced(
            FunctionTemplateStructuralReferenceTag<
                FunctionTemplateStructuralReference{
                    function_template_structural_target_one}>{}) != 37 ||
        function_template_structural_reference_deduced(
            FunctionTemplateStructuralReferenceTag<
                FunctionTemplateStructuralReference{
                    function_template_structural_target_two}>{}) != 41 ||
        function_template_structural_pair_explicit<
            FunctionTemplateStructuralPair{12, 5}>() != 1205 ||
        function_template_structural_pair_deduced(
            FunctionTemplateStructuralPairTag<
                FunctionTemplateStructuralPair{8, 3}>{}) != 803 ||
        function_template_structural_union_value_explicit<
            FunctionTemplateStructuralUnion{.value = 17}>() != 17 ||
        function_template_structural_union_value_explicit<
            FunctionTemplateStructuralUnion{17}>() != 17 ||
        function_template_structural_union_value_deduced(
            FunctionTemplateStructuralUnionTag<
                FunctionTemplateStructuralUnion{.value = 19}>{}) != 19 ||
        function_template_structural_union_tag_explicit<
            FunctionTemplateStructuralUnion{.tag = 5}>() != 5 ||
        function_template_structural_union_tag_deduced(
            FunctionTemplateStructuralUnionTag<
                FunctionTemplateStructuralUnion{.tag = 7}>{}) != 7 ||
        function_template_structural_nested_union_value_explicit<
            FunctionTemplateStructuralUnionContainer{
                2, {.value = 19}, 3}>() != 20193 ||
        function_template_structural_nested_union_value_explicit<
            FunctionTemplateStructuralUnionContainer{
                2, {19}, 3}>() != 20193 ||
        function_template_structural_nested_union_value_deduced(
            FunctionTemplateStructuralUnionContainerTag<
                FunctionTemplateStructuralUnionContainer{
                    2, {19}, 4}>{}) != 20194 ||
        function_template_structural_nested_union_tag_explicit<
            FunctionTemplateStructuralUnionContainer{
                5, {.tag = 7}, 6}>() != 50076 ||
        function_template_structural_nested_union_tag_deduced(
            FunctionTemplateStructuralUnionContainerTag<
                FunctionTemplateStructuralUnionContainer{
                    5, {.tag = 8}, 9}>{}) != 50089 ||
        function_template_structural_union_array_value_explicit<
            FunctionTemplateStructuralUnionArray{
                {{.value = 2}, {.value = 3}}, 4}>() != 324 ||
        function_template_structural_union_array_value_explicit<
            FunctionTemplateStructuralUnionArray{
                {{2}, {3}}, 4}>() != 324 ||
        function_template_structural_union_array_value_deduced(
            FunctionTemplateStructuralUnionArrayTag<
                FunctionTemplateStructuralUnionArray{
                    {{2}, {.value = 5}}, 6}>{}) != 526 ||
        function_template_structural_union_array_tag_explicit<
            FunctionTemplateStructuralUnionArray{
                {{.tag = 7}, {.tag = 8}}, 9}>() != 879 ||
        function_template_structural_union_array_tag_deduced(
            FunctionTemplateStructuralUnionArrayTag<
                FunctionTemplateStructuralUnionArray{
                    {{.tag = 1}, {.tag = 4}}, 2}>{}) != 412 ||
        function_template_structural_union_matrix_value_explicit<
            FunctionTemplateStructuralUnionMatrix{
                {{{.value = 1}, {.value = 2}},
                 {{.value = 3}, {.value = 4}}}, 5}>() != 325 ||
        function_template_structural_union_matrix_value_explicit<
            FunctionTemplateStructuralUnionMatrix{
                {{{1}, {2}}, {{3}, {4}}}, 5}>() != 325 ||
        function_template_structural_union_matrix_value_deduced(
            FunctionTemplateStructuralUnionMatrixTag<
                FunctionTemplateStructuralUnionMatrix{
                    {{{1}, {.value = 5}},
                     {{.value = 6}, {7}}}, 8}>{}) != 658 ||
        function_template_structural_union_matrix_tag_explicit<
            FunctionTemplateStructuralUnionMatrix{
                {{{.tag = 1}, {.tag = 2}},
                 {{.tag = 3}, {.tag = 4}}}, 5}>() != 325 ||
        function_template_structural_union_matrix_tag_deduced(
            FunctionTemplateStructuralUnionMatrixTag<
                FunctionTemplateStructuralUnionMatrix{
                    {{{.tag = 5}, {.tag = 6}},
                     {{.tag = 7}, {.tag = 8}}}, 9}>{}) != 769 ||
        function_template_structural_union_tensor_explicit<
            FunctionTemplateStructuralUnionTensor{
                {{{{.value = 1}, {.tag = 2}},
                  {{.tag = 3}, {.tag = 4}}},
                 {{{.tag = 5}, {.value = 6}},
                  {{.tag = 7}, {.value = 8}}}}, 9}>() != 639 ||
        function_template_structural_union_tensor_deduced(
            FunctionTemplateStructuralUnionTensorTag<
                FunctionTemplateStructuralUnionTensor{
                    {{{{.value = 2}, {.tag = 3}},
                      {{.tag = 4}, {.tag = 5}}},
                     {{{.tag = 6}, {.value = 7}},
                     {{.tag = 8}, {.value = 9}}}}, 1}>{}) != 741 ||
        function_template_structural_union_quad_array_explicit<
            FunctionTemplateStructuralUnionQuadArray{
                .payloads = {
                    {
                        {
                            {{.tag = 2}, {.value = 3}},
                            {{.value = 4}, {.tag = 5}}},
                        {
                            {{.tag = 6}, {.value = 7}},
                            {{.value = 8}, {.tag = 9}}}},
                    {
                        {
                            {{.tag = 10}, {.value = 11}},
                            {{.value = 12}, {.tag = 13}}},
                        {
                            {{.tag = 14}, {.value = 15}},
                            {{.value = 16}, {.tag = 17}}}}},
                .suffix = 9}>() != 12079 ||
        function_template_structural_union_quad_array_deduced(
            FunctionTemplateStructuralUnionQuadArrayTag<
                FunctionTemplateStructuralUnionQuadArray{
                    {
                        {
                            {
                                {{.tag = 18}, {.value = 19}},
                                {{.value = 20}, {.tag = 21}}},
                            {
                                {{.tag = 22}, {.value = 23}},
                                {{.value = 24}, {.tag = 25}}}},
                        {
                            {
                                {{.tag = 26}, {.value = 27}},
                                {{.value = 28}, {.tag = 29}}},
                            {
                                {{.tag = 30}, {.value = 31}},
                                {{.value = 32}, {.tag = 33}}}}},
                    1}>{}) != 28231 ||
        function_template_structural_union_five_dimensional_array_explicit<
            function_template_make_structural_five_d_union_value(
                3, 7, 4, 17, 9)>() != 1779 ||
        function_template_structural_union_five_dimensional_array_deduced(
            FunctionTemplateStructuralUnionFiveDimensionalArrayTag<
                function_template_make_structural_five_d_union_value(
                    5, 8, 6, 18, 1)>{}) != 1881 ||
        function_template_structural_union_five_dimensional_array_explicit<
            function_template_make_structural_five_d_union_template_value<
                2>(3, 7, 4, 17, 9)>() != 1979 ||
        function_template_structural_union_five_dimensional_array_deduced(
            FunctionTemplateStructuralUnionFiveDimensionalArrayTag<
                function_template_make_structural_five_d_union_template_value<
                    3>(5, 8, 6, 18, 1)>{}) != 2181 ||
        function_template_structural_union_five_dimensional_array_explicit<
            function_template_make_structural_five_d_union_deduced_value(
                4, 6, 5, 12, 8)>() != 1468 ||
        function_template_structural_union_five_dimensional_array_deduced(
            FunctionTemplateStructuralUnionFiveDimensionalArrayTag<
                function_template_make_structural_five_d_union_deduced_value(
                    static_cast<unsigned char>(2), 9, 4, 14, 3)>{}) != 1693 ||
        function_template_structural_union_float_explicit<
            FunctionTemplateStructuralFloatingUnion{
                .single = 1.5f}>() != 15 ||
        function_template_structural_union_double_deduced(
            FunctionTemplateStructuralFloatingUnionTag<
                FunctionTemplateStructuralFloatingUnion{
                    .wide = 2.25}>{}) != 22 ||
        function_template_structural_union_long_double_explicit<
            FunctionTemplateStructuralFloatingUnion{
                .extended = 1.25L}>() != 125 ||
        function_template_structural_union_long_double_deduced(
            FunctionTemplateStructuralFloatingUnionTag<
                FunctionTemplateStructuralFloatingUnion{
                    .extended = 2.5L}>{}) != 250 ||
        function_template_structural_union_nested_pointer_explicit<
            FunctionTemplateStructuralPointerUnion{
                .object = {
                    &function_template_structural_union_pointer_target,
                    4}}>() != 27 ||
        function_template_structural_union_nested_pointer_deduced(
            FunctionTemplateStructuralPointerUnionTag<
                FunctionTemplateStructuralPointerUnion{
                    .object = {
                        &function_template_structural_union_pointer_target,
                        5}}>{}) !=
            28 ||
        function_template_structural_pointer_factory_explicit<
            function_template_make_structural_pointer_union_value(
                &function_template_structural_union_pointer_target,
                8)>() != 31 ||
        function_template_structural_pointer_factory_explicit<
            function_template_make_structural_pointer_union_template_value<
                2>(&function_template_structural_union_pointer_target,
                   4)>() != 29 ||
        function_template_structural_pointer_factory_deduced(
            FunctionTemplateStructuralPointerFactoryValueTag<
                function_template_make_structural_pointer_union_deduced_value(
                    &function_template_structural_union_pointer_target,
                    7)>{}) != 30 ||
        function_template_structural_union_direct_pointer_explicit<
            FunctionTemplateStructuralDirectPointerUnion{
                .pointer =
                    &function_template_structural_union_pointer_target}>() !=
            23 ||
        function_template_structural_union_direct_pointer_deduced(
            FunctionTemplateStructuralDirectPointerUnionTag<
                FunctionTemplateStructuralDirectPointerUnion{
                    .pointer =
                        &function_template_structural_union_pointer_target}>{}) !=
            23 ||
        function_template_structural_union_null_pointer_explicit<
            FunctionTemplateStructuralDirectPointerUnion{
                .pointer = nullptr}>() != 1 ||
        function_template_structural_union_nested_pointer_explicit<
            FunctionTemplateStructuralPointerUnion{
                .object = {
                    &function_template_structural_union_pointer_target,
                    4}}>() != 27 ||
        function_template_structural_union_nested_pointer_deduced(
            FunctionTemplateStructuralPointerUnionTag<
                FunctionTemplateStructuralPointerUnion{
                    .object = {
                        &function_template_structural_union_pointer_target,
                        5}}>{}) != 28 ||
        function_template_structural_union_direct_pointer_explicit<
            FunctionTemplateStructuralDirectPointerUnion{
                .pointer =
                    &function_template_structural_union_pointer_target}>() !=
            23 ||
        function_template_structural_union_direct_pointer_deduced(
            FunctionTemplateStructuralDirectPointerUnionTag<
                FunctionTemplateStructuralDirectPointerUnion{
                    .pointer =
                        &function_template_structural_union_pointer_target}>{}) !=
            23 ||
        function_template_structural_pointer_array_explicit<
            FunctionTemplateStructuralPointerArray{
                .values = {
                    &function_template_structural_pointer_array_first,
                    &function_template_structural_pointer_array_second},
                .bias = 7}>() != 1597 ||
        function_template_structural_pointer_array_deduced(
            FunctionTemplateStructuralPointerArrayTag<
                FunctionTemplateStructuralPointerArray{
                    {&function_template_structural_pointer_array_second,
                     &function_template_structural_pointer_array_first},
                    5}>{}) != 3035 ||
        function_template_structural_long_double_member_explicit<
            FunctionTemplateStructuralLongDoubleMember{1.25L, 7}>() != 7 ||
        function_template_structural_long_double_member_deduced(
            FunctionTemplateStructuralLongDoubleMemberTag<
                FunctionTemplateStructuralLongDoubleMember{2.5L, 5}>{}) != 5 ||
        function_template_structural_long_double_array_explicit<
            FunctionTemplateStructuralLongDoubleArray{
                .values = {1.25L, 2.5L}, .bias = 7}>() != 7 ||
        function_template_structural_long_double_array_deduced(
            FunctionTemplateStructuralLongDoubleArrayTag<
                FunctionTemplateStructuralLongDoubleArray{
                    {2.5L, 1.25L}, 5}>{}) != 5 ||
        function_template_structural_default_member_explicit<
            FunctionTemplateStructuralDefaultMember{.value = 4}>() != 46 ||
        function_template_structural_default_member_deduced(
            FunctionTemplateStructuralDefaultMemberTag<
                FunctionTemplateStructuralDefaultMember{5}>{}) != 56 ||
        function_template_structural_union_array_explicit<
            FunctionTemplateStructuralArrayUnion{
                .values = {4, 2}}>() != 42 ||
        function_template_structural_union_array_deduced(
            FunctionTemplateStructuralArrayUnionTag<
                FunctionTemplateStructuralArrayUnion{
                    {5, 3}}>{}) != 53 ||
        function_template_structural_union_scalar_alternative_explicit<
            FunctionTemplateStructuralArrayUnion{
                .scalar = 4}>() != 42 ||
        function_template_structural_union_matrix_array_explicit<
            FunctionTemplateStructuralMatrixArrayUnion{
                .values = {{1, 2}, {3, 4}}}>() != 3241 ||
        function_template_structural_union_matrix_array_deduced(
            FunctionTemplateStructuralMatrixArrayUnionTag<
                FunctionTemplateStructuralMatrixArrayUnion{
                    {{5, 6}, {7, 8}}}>{}) != 7685 ||
        function_template_structural_union_tensor_array_explicit<
            FunctionTemplateStructuralTensorArrayUnion{
                .values = {{{1, 2}, {3, 4}}, {{5, 6}, {7, 8}}}}>() !=
            12345678 ||
        function_template_structural_union_tensor_array_deduced(
            FunctionTemplateStructuralTensorArrayUnionTag<
                FunctionTemplateStructuralTensorArrayUnion{
                    {{{8, 7}, {6, 5}}, {{4, 3}, {2, 1}}}}>{}) != 87654321 ||
        function_template_structural_union_quad_array_alternative_explicit<
            FunctionTemplateStructuralQuadArrayUnion{
                .values = {{{{1, 2}, {3, 4}},
                            {{5, 6}, {7, 8}}},
                           {{{9, 10}, {11, 12}},
                            {{13, 14}, {15, 16}}}}}>() != 11076 ||
        function_template_structural_union_quad_array_alternative_deduced(
            FunctionTemplateStructuralQuadArrayUnionTag<
                FunctionTemplateStructuralQuadArrayUnion{
                    {{{{16, 15}, {14, 13}},
                      {{12, 11}, {10, 9}}},
                     {{{8, 7}, {6, 5}},
                      {{4, 3}, {2, 1}}}}}>{}) != 6111 ||
        function_template_structural_mixed_union_object_explicit<
            FunctionTemplateStructuralMixedUnionContainer{
                {.object = {4, 7}}, 2}>() != 472 ||
        function_template_structural_mixed_union_object_explicit<
            FunctionTemplateStructuralMixedUnionContainer{
                {.object = {.code = 4, .extra = 7}}, 2}>() != 472 ||
        function_template_structural_mixed_union_object_deduced(
            FunctionTemplateStructuralMixedUnionTag<
                FunctionTemplateStructuralMixedUnionContainer{
                    {.object = {5, 6}}, 3}>{}) != 563 ||
        function_template_structural_mixed_union_scalar_explicit<
            FunctionTemplateStructuralMixedUnionContainer{
                {.scalar = 4}, 2}>() != 42 ||
        function_template_structural_derived_explicit<
            FunctionTemplateStructuralDerived{
                {}, {{11, 23}}, {2}, {}, {{}, {}}, 7, {},
                {{}, {}}}>() != 22307 ||
        function_template_structural_derived_deduced(
            FunctionTemplateStructuralDerivedTag<
                FunctionTemplateStructuralDerived{
                    {}, {{4, 9}}, {3}, {}, {{}, {}}, 4, {},
                    {{}, {}}}>{}) != 30904 ||
        function_template_structural_pair_explicit<
            FunctionTemplateStructuralPair{6 + 6, 2 + 3}>() != 1205 ||
        function_template_structural_derived_explicit<
            FunctionTemplateStructuralDerived{
                {}, {{1 + 10, 20 + 3}}, {1 + 1}, {}, {{}, {}},
                4 + 3, {}, {{}, {}}}>() != 22307 ||
        function_template_structural_union_quint_array_explicit<
            FUNCTION_TEMPLATE_STRUCTURAL_QUINT_ARRAY_VALUE>() != 22142 ||
        function_template_structural_union_quint_array_deduced(
            FunctionTemplateStructuralQuintArrayUnionTag<
                FUNCTION_TEMPLATE_STRUCTURAL_QUINT_ARRAY_VALUE>{}) != 22142) {
        return 1;
    }
    return 0;
}

#undef FUNCTION_TEMPLATE_STRUCTURAL_QUINT_ARRAY_VALUE
