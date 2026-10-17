int structural_throwing_function(int value) {
    return value + 1;
}

using StructuralNoexceptFunction = int (*)(int) noexcept;

struct StructuralNoexceptFunctionKey {
    StructuralNoexceptFunction pointer;
};

template <StructuralNoexceptFunctionKey Value>
struct StructuralNoexceptFunctionOwner {};

StructuralNoexceptFunctionOwner<
    StructuralNoexceptFunctionKey{&structural_throwing_function}> invalid;
