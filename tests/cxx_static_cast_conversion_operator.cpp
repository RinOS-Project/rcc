class ImplicitIntegerConversion {
public:
    int value;
    int calls;

    operator int() {
        ++calls;
        return value + 2;
    }
};

class ExplicitIntegerConversion {
public:
    int value;
    int calls;

    explicit operator int() {
        ++calls;
        return value + 3;
    }
};

class ExplicitBooleanConversion {
public:
    int value;
    int calls;

    explicit operator bool() {
        ++calls;
        return value == 2;
    }
};

class ExplicitPointerConversion {
public:
    int* pointer;
    int calls;

    explicit operator int*() {
        ++calls;
        return pointer;
    }
};

struct MemberPointerTarget {
    int value;
};

using MemberPointerTargetValue = int MemberPointerTarget::*;

class ExplicitMemberPointerConversion {
public:
    int calls;

    explicit operator MemberPointerTargetValue() {
        ++calls;
        return &MemberPointerTarget::value;
    }
};

struct MemberPointerConversionPrefix {
    int prefix;
};

struct MemberPointerConversionBase {
    int value;
};

struct MemberPointerConversionDerived : MemberPointerConversionPrefix,
                                       MemberPointerConversionBase {};

class BaseMemberPointerConversion {
public:
    operator int MemberPointerConversionBase::*() {
        return &MemberPointerConversionBase::value;
    }
};

#if defined(TEST_EXPLICIT_STANDARD_SEQUENCE_REJECTED)
unsigned reject_explicit_standard_conversion(
    ExplicitIntegerConversion value) {
    return static_cast<unsigned>(value);
}
#endif

#if defined(TEST_IMPLICIT_CONVERSION_REJECTED)
int reject_implicit_conversion(ExplicitIntegerConversion value) {
    return value;
}

MemberPointerTargetValue reject_implicit_member_pointer_conversion(
    ExplicitMemberPointerConversion value) {
    return value;
}
#endif

class RankedConversion {
public:
    int integer_calls;
    int floating_calls;

    operator int() {
        ++integer_calls;
        return 5;
    }

    explicit operator double() {
        ++floating_calls;
        return 2.5;
    }
};

int main() {
    ImplicitIntegerConversion implicit_value{7, 0};
    ExplicitIntegerConversion explicit_value{13, 0};
    ExplicitBooleanConversion explicit_boolean{1, 0};
    int pointed_value = 29;
    ExplicitPointerConversion explicit_pointer{&pointed_value, 0};
    MemberPointerTarget member_pointer_target{31};
    ExplicitMemberPointerConversion explicit_member_pointer{0};
    MemberPointerConversionDerived member_pointer_derived;
    member_pointer_derived.prefix = 5;
    member_pointer_derived.value = 37;
    BaseMemberPointerConversion base_member_pointer;
    RankedConversion ranked_value{0, 0};
    MemberPointerTargetValue member_pointer =
        static_cast<MemberPointerTargetValue>(explicit_member_pointer);
    int MemberPointerConversionDerived::*adjusted_member_pointer =
        static_cast<int MemberPointerConversionDerived::*>(
            base_member_pointer);

    return static_cast<int>(implicit_value) == 9 &&
                   static_cast<unsigned>(implicit_value) == 9u &&
                   implicit_value.calls == 2 &&
                   static_cast<int>(ImplicitIntegerConversion{8, 0}) == 10 &&
                   static_cast<int>(explicit_value) == 16 &&
                   explicit_value.calls == 1 &&
                   !static_cast<bool>(explicit_boolean) &&
                   explicit_boolean.calls == 1 &&
                   static_cast<int*>(explicit_pointer) == &pointed_value &&
                   explicit_pointer.calls == 1 &&
                   (member_pointer_target.*member_pointer) == 31 &&
                   explicit_member_pointer.calls == 1 &&
                   (member_pointer_derived.*adjusted_member_pointer) == 37 &&
                   static_cast<double>(ranked_value) == 2.5 &&
                   ranked_value.integer_calls == 0 &&
                   ranked_value.floating_calls == 1 &&
                   static_cast<int>(ranked_value) == 5 &&
                   ranked_value.integer_calls == 1 &&
                   ranked_value.floating_calls == 1
        ? 0
        : 1;
}
