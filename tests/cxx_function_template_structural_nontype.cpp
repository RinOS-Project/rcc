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

struct FunctionTemplateStructuralUnionContainer {
    unsigned char prefix;
    FunctionTemplateStructuralNestedUnion payload;
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

template<FunctionTemplateStructuralUnionContainer Value>
struct FunctionTemplateStructuralUnionContainerTag {};

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
