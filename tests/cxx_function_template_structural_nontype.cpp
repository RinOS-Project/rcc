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
};

union FunctionTemplateStructuralArrayUnion {
    int values[2];
    int scalar;
};

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

template<FunctionTemplateStructuralPointerUnion Value>
struct FunctionTemplateStructuralPointerUnionTag {};

template<FunctionTemplateStructuralDirectPointerUnion Value>
struct FunctionTemplateStructuralDirectPointerUnionTag {};

template<FunctionTemplateStructuralUnionContainer Value>
struct FunctionTemplateStructuralUnionContainerTag {};

template<FunctionTemplateStructuralUnionArray Value>
struct FunctionTemplateStructuralUnionArrayTag {};

template<FunctionTemplateStructuralUnionMatrix Value>
struct FunctionTemplateStructuralUnionMatrixTag {};

template<FunctionTemplateStructuralUnionTensor Value>
struct FunctionTemplateStructuralUnionTensorTag {};

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

template<FunctionTemplateStructuralFloatingUnion Value>
constexpr int function_template_structural_union_float_explicit() {
    return Value.single == 1.5f ? 15 : 0;
}

template<FunctionTemplateStructuralFloatingUnion Value>
constexpr int function_template_structural_union_double_deduced(
    FunctionTemplateStructuralFloatingUnionTag<Value>) {
    return Value.wide == 2.25 ? 22 : 0;
}

template<FunctionTemplateStructuralPointerUnion Value>
constexpr int
function_template_structural_union_nested_pointer_explicit() {
    return Value.object.bias + 23;
}

template<FunctionTemplateStructuralPointerUnion Value>
constexpr int function_template_structural_union_nested_pointer_deduced(
    FunctionTemplateStructuralPointerUnionTag<Value>) {
    return Value.object.bias + 23;
}

template<FunctionTemplateStructuralDirectPointerUnion Value>
constexpr int
function_template_structural_union_direct_pointer_explicit() {
    return 23;
}

template<FunctionTemplateStructuralDirectPointerUnion Value>
constexpr int function_template_structural_union_direct_pointer_deduced(
    FunctionTemplateStructuralDirectPointerUnionTag<Value>) {
    return 23;
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
static_assert(function_template_structural_union_float_explicit<
                  FunctionTemplateStructuralFloatingUnion{
                      .single = 1.5f}>() == 15);
static_assert(function_template_structural_union_double_deduced(
                  FunctionTemplateStructuralFloatingUnionTag<
                      FunctionTemplateStructuralFloatingUnion{
                          .wide = 2.25}>{}) == 22);
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
int main() {
    if (function_template_structural_value_explicit<
            FunctionTemplateStructuralTarget{37}>() != 37 ||
        function_template_structural_value_deduced(
            FunctionTemplateStructuralValueTag<
                FunctionTemplateStructuralTarget{41}>{}) != 41 ||
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
        function_template_structural_union_float_explicit<
            FunctionTemplateStructuralFloatingUnion{
                .single = 1.5f}>() != 15 ||
        function_template_structural_union_double_deduced(
            FunctionTemplateStructuralFloatingUnionTag<
                FunctionTemplateStructuralFloatingUnion{
                    .wide = 2.25}>{}) != 22 ||
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
                4 + 3, {}, {{}, {}}}>() != 22307) {
        return 1;
    }
    return 0;
}
