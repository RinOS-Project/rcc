using FunctionReferenceTarget = int();

int lvalue_function_target() {
    return 97;
}

int rvalue_function_target() {
    return 101;
}

class FunctionReferenceSource {
public:
    operator FunctionReferenceTarget&() const {
        return lvalue_function_target;
    }

    operator FunctionReferenceTarget&&() const {
        return static_cast<FunctionReferenceTarget&&>(
            rvalue_function_target);
    }
};
