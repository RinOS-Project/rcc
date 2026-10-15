template <int... OuterValues>
struct NestedOwnerNontypeFoldPackMismatch {
    template <int... InnerValues>
    struct Inner {
        static bool values_match() {
            return ((OuterValues == InnerValues) && ...);
        }
    };
};

int main() {
    using mismatched_inner =
        NestedOwnerNontypeFoldPackMismatch<3, 7>::Inner<3>;
    mismatched_inner value;
    return value.values_match() ? 0 : 1;
}
