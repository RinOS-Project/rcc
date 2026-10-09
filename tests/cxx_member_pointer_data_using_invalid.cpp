struct MemberPointerUsingAmbiguousA {
    int value;
};

struct MemberPointerUsingAmbiguousB {
    int value;
};

struct MemberPointerUsingAmbiguousDerived : MemberPointerUsingAmbiguousA,
                                            MemberPointerUsingAmbiguousB {};

int MemberPointerUsingAmbiguousA::*ambiguous_member =
    &MemberPointerUsingAmbiguousDerived::value;

struct MemberPointerRepeatedBase {
    int repeated;
};

struct MemberPointerRepeatedLeft : MemberPointerRepeatedBase {};
struct MemberPointerRepeatedRight : MemberPointerRepeatedBase {};

struct MemberPointerRepeatedDerived : MemberPointerRepeatedLeft,
                                     MemberPointerRepeatedRight {};

int MemberPointerRepeatedBase::*ambiguous_repeated_member =
    &MemberPointerRepeatedDerived::repeated;

struct MemberPointerMixedFieldBase {
    int mixed;
};

struct MemberPointerMixedFunctionBase {
    int mixed() { return 1; }
};

struct MemberPointerMixedFieldFunctionDerived
    : MemberPointerMixedFieldBase, MemberPointerMixedFunctionBase {};

int MemberPointerMixedFieldBase::*ambiguous_mixed_member =
    &MemberPointerMixedFieldFunctionDerived::mixed;
