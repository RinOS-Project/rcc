struct LeftMemberFunctionBase {
    int apply(int value) { return value; }
};

struct RightMemberFunctionBase {
    int apply(int value) { return value + 1; }
};

struct AmbiguousMemberFunctionDerived
    : LeftMemberFunctionBase, RightMemberFunctionBase {};

int (AmbiguousMemberFunctionDerived::*ambiguous_method)(int) =
    &AmbiguousMemberFunctionDerived::apply;

struct AmbiguousOwnerBase {
    int adjust(int value) { return value; }
};

struct AmbiguousOwnerLeft : AmbiguousOwnerBase {};
struct AmbiguousOwnerRight : AmbiguousOwnerBase {};
struct AmbiguousOwnerDerived : AmbiguousOwnerLeft, AmbiguousOwnerRight {};

int (AmbiguousOwnerBase::*base_method)(int) =
    &AmbiguousOwnerBase::adjust;
int (AmbiguousOwnerDerived::*ambiguous_owner_method)(int) = base_method;
