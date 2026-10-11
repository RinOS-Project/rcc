template<int Value>
struct FunctionTemplateExpressionValuePackTag {};

template<int... Values>
int select_non_deduced_nttp_pack(
    FunctionTemplateExpressionValuePackTag<(Values + 1)>...) {
    return 1;
}

int main() {
    return select_non_deduced_nttp_pack(
        FunctionTemplateExpressionValuePackTag<2>{});
}
