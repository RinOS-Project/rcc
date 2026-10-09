struct PrivateInheritedMemberFunctionBase {
    int read() {
        return 7;
    }
};

struct PrivateInheritedMemberFunctionDerived
    : private PrivateInheritedMemberFunctionBase {};

int (PrivateInheritedMemberFunctionBase::*inaccessible_method)() =
    &PrivateInheritedMemberFunctionDerived::read;
