#if !defined(__RCC__)
#include <type_traits>
#endif

struct InheritedFallbackPrefix {
    int prefix;
};

struct InheritedFallbackSubobject {
    int value;
    InheritedFallbackSubobject& operator=(
        const InheritedFallbackSubobject&) = default;
    InheritedFallbackSubobject& operator=(
        InheritedFallbackSubobject&&) = delete;
};

struct InheritedFallbackBase {
    InheritedFallbackSubobject value;
    InheritedFallbackBase& operator=(
        const InheritedFallbackBase&) = default;
    InheritedFallbackBase& operator=(InheritedFallbackBase&&) = default;
};

struct InheritedFallbackDerived : InheritedFallbackPrefix,
                                  InheritedFallbackBase {
    using InheritedFallbackBase::operator=;
};

static int inherited_fallback_copy_calls;

struct InheritedNontrivialFallbackSubobject {
    int value;
    InheritedNontrivialFallbackSubobject& operator=(
        const InheritedNontrivialFallbackSubobject& source) {
        value = source.value;
        ++inherited_fallback_copy_calls;
        return *this;
    }
    InheritedNontrivialFallbackSubobject& operator=(
        InheritedNontrivialFallbackSubobject&&) = delete;
};

struct InheritedNontrivialFallbackBase {
    InheritedNontrivialFallbackSubobject value;
    InheritedNontrivialFallbackBase& operator=(
        const InheritedNontrivialFallbackBase&) = default;
    InheritedNontrivialFallbackBase& operator=(
        InheritedNontrivialFallbackBase&&) = default;
};

struct InheritedNontrivialFallbackDerived : InheritedFallbackPrefix,
                                             InheritedNontrivialFallbackBase {
    using InheritedNontrivialFallbackBase::operator=;
};

struct DirectNontrivialDefaultedAssignment {
    InheritedNontrivialFallbackSubobject value;
    DirectNontrivialDefaultedAssignment& operator=(
        const DirectNontrivialDefaultedAssignment&) = default;
    DirectNontrivialDefaultedAssignment& operator=(
        DirectNontrivialDefaultedAssignment&&) = default;
};

struct DefaultedNontrivialBaseAssignment {
    InheritedNontrivialFallbackSubobject value;
    DefaultedNontrivialBaseAssignment& operator=(
        const DefaultedNontrivialBaseAssignment&) = default;
    DefaultedNontrivialBaseAssignment& operator=(
        DefaultedNontrivialBaseAssignment&&) = default;
};

struct DerivedDefaultedNontrivialAssignment :
    DefaultedNontrivialBaseAssignment {
    int prefix;
    int tail;
    DerivedDefaultedNontrivialAssignment& operator=(
        const DerivedDefaultedNontrivialAssignment&) = default;
    DerivedDefaultedNontrivialAssignment& operator=(
        DerivedDefaultedNontrivialAssignment&&) = default;
};

static int defaulted_move_assignment_calls;

struct MoveTrackedAssignmentSubobject {
    int value;
    MoveTrackedAssignmentSubobject& operator=(
        const MoveTrackedAssignmentSubobject& source) {
        value = source.value;
        ++inherited_fallback_copy_calls;
        return *this;
    }
    MoveTrackedAssignmentSubobject& operator=(
        MoveTrackedAssignmentSubobject&& source) {
        value = source.value;
        ++defaulted_move_assignment_calls;
        return *this;
    }
};

struct DirectDefaultedMoveAssignment {
    MoveTrackedAssignmentSubobject value;
    DirectDefaultedMoveAssignment& operator=(
        const DirectDefaultedMoveAssignment&) = default;
    DirectDefaultedMoveAssignment& operator=(
        DirectDefaultedMoveAssignment&&) = default;
};

#if defined(__RCC__)
static_assert(__is_assignable(
    InheritedFallbackDerived&, InheritedFallbackBase&&));
static_assert(__is_assignable(
    InheritedNontrivialFallbackDerived&,
    InheritedNontrivialFallbackBase&&));
static_assert(__is_assignable(
    DirectNontrivialDefaultedAssignment&,
    DirectNontrivialDefaultedAssignment&&));
static_assert(__is_assignable(
    DerivedDefaultedNontrivialAssignment&,
    DerivedDefaultedNontrivialAssignment&&));
static_assert(__is_assignable(
    DirectDefaultedMoveAssignment&,
    DirectDefaultedMoveAssignment&&));
#else
static_assert(std::is_assignable_v<
    InheritedFallbackDerived&, InheritedFallbackBase&&>);
static_assert(std::is_assignable_v<
    InheritedNontrivialFallbackDerived&,
    InheritedNontrivialFallbackBase&&>);
static_assert(std::is_assignable_v<
    DirectNontrivialDefaultedAssignment&,
    DirectNontrivialDefaultedAssignment&&>);
static_assert(std::is_assignable_v<
    DerivedDefaultedNontrivialAssignment&,
    DerivedDefaultedNontrivialAssignment&&>);
static_assert(std::is_assignable_v<
    DirectDefaultedMoveAssignment&,
    DirectDefaultedMoveAssignment&&>);
#endif

int main() {
    InheritedFallbackDerived destination;
    InheritedFallbackBase source;
    destination.prefix = 7;
    destination.value.value = 11;
    source.value.value = 29;
    destination = static_cast<InheritedFallbackBase&&>(source);
    if (destination.prefix != 7 || destination.value.value != 29) return 1;

    InheritedNontrivialFallbackDerived nontrivial_destination;
    InheritedNontrivialFallbackBase nontrivial_source;
    nontrivial_destination.prefix = 13;
    nontrivial_destination.value.value = 17;
    nontrivial_source.value.value = 31;
    inherited_fallback_copy_calls = 0;
    nontrivial_destination = static_cast<
        InheritedNontrivialFallbackBase&&>(nontrivial_source);
    if (nontrivial_destination.prefix != 13 ||
        nontrivial_destination.value.value != 31 ||
        inherited_fallback_copy_calls != 1) return 2;

    InheritedNontrivialFallbackDerived nontrivial_derived_source;
    nontrivial_derived_source.prefix = 19;
    nontrivial_derived_source.value.value = 23;
    inherited_fallback_copy_calls = 0;
    nontrivial_destination = static_cast<
        InheritedNontrivialFallbackBase&&>(nontrivial_derived_source);
    if (nontrivial_destination.prefix != 13 ||
        nontrivial_destination.value.value != 23 ||
        inherited_fallback_copy_calls != 1) return 3;

    DirectNontrivialDefaultedAssignment direct_destination;
    DirectNontrivialDefaultedAssignment direct_source;
    direct_destination.value.value = 37;
    direct_source.value.value = 41;
    inherited_fallback_copy_calls = 0;
    direct_destination = static_cast<
        DirectNontrivialDefaultedAssignment&&>(direct_source);
    if (direct_destination.value.value != 41 ||
        inherited_fallback_copy_calls != 1) return 4;

    DerivedDefaultedNontrivialAssignment derived_destination;
    DerivedDefaultedNontrivialAssignment derived_source;
    derived_destination.prefix = 43;
    derived_destination.value.value = 47;
    derived_destination.tail = 53;
    derived_source.prefix = 59;
    derived_source.value.value = 61;
    derived_source.tail = 67;
    inherited_fallback_copy_calls = 0;
    derived_destination = static_cast<
        DerivedDefaultedNontrivialAssignment&&>(derived_source);
    if (derived_destination.prefix != 59 ||
        derived_destination.value.value != 61 ||
        derived_destination.tail != 67 ||
        inherited_fallback_copy_calls != 1) return 5;

    DirectDefaultedMoveAssignment move_destination;
    DirectDefaultedMoveAssignment move_source;
    move_destination.value.value = 71;
    move_source.value.value = 73;
    inherited_fallback_copy_calls = 0;
    defaulted_move_assignment_calls = 0;
    move_destination = static_cast<
        DirectDefaultedMoveAssignment&&>(move_source);
    return move_destination.value.value == 73 &&
           inherited_fallback_copy_calls == 0 &&
           defaulted_move_assignment_calls == 1 ? 0 : 6;
}
