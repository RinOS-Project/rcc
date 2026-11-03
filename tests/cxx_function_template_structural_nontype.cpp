struct FunctionTemplateStructuralTarget {
    int value;
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
                    function_template_structural_target_two}>{}) != 41) {
        return 1;
    }
    return 0;
}
