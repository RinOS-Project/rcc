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

static int virtual_base_copy_assignment_calls;
static int virtual_base_move_assignment_calls;

struct TrackedVirtualAssignmentBase {
    int value;
    TrackedVirtualAssignmentBase& operator=(
        const TrackedVirtualAssignmentBase& source) {
        value = source.value;
        ++virtual_base_copy_assignment_calls;
        return *this;
    }
    TrackedVirtualAssignmentBase& operator=(
        TrackedVirtualAssignmentBase&& source) {
        value = source.value;
        ++virtual_base_move_assignment_calls;
        return *this;
    }
};

#if !defined(__RCC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvirtual-move-assign"
#endif
struct DirectVirtualDefaultedAssignment :
    virtual TrackedVirtualAssignmentBase {
    int tail;
    DirectVirtualDefaultedAssignment& operator=(
        const DirectVirtualDefaultedAssignment&) = default;
    DirectVirtualDefaultedAssignment& operator=(
        DirectVirtualDefaultedAssignment&&) = default;
};
#if !defined(__RCC__)
#pragma GCC diagnostic pop
#endif

#if !defined(__RCC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvirtual-move-assign"
#endif
struct ExtendedVirtualDefaultedAssignment :
    DirectVirtualDefaultedAssignment {
    int extra;
    ExtendedVirtualDefaultedAssignment& operator=(
        const ExtendedVirtualDefaultedAssignment&) = default;
    ExtendedVirtualDefaultedAssignment& operator=(
        ExtendedVirtualDefaultedAssignment&&) = default;
};
#if !defined(__RCC__)
#pragma GCC diagnostic pop
#endif

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
static_assert(__is_assignable(
    DirectVirtualDefaultedAssignment&,
    const DirectVirtualDefaultedAssignment&));
static_assert(__is_assignable(
    DirectVirtualDefaultedAssignment&,
    DirectVirtualDefaultedAssignment&&));
static_assert(__is_assignable(
    ExtendedVirtualDefaultedAssignment&,
    const ExtendedVirtualDefaultedAssignment&));
static_assert(__is_assignable(
    ExtendedVirtualDefaultedAssignment&,
    ExtendedVirtualDefaultedAssignment&&));
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
static_assert(std::is_assignable_v<
    DirectVirtualDefaultedAssignment&,
    const DirectVirtualDefaultedAssignment&>);
static_assert(std::is_assignable_v<
    DirectVirtualDefaultedAssignment&,
    DirectVirtualDefaultedAssignment&&>);
static_assert(std::is_assignable_v<
    ExtendedVirtualDefaultedAssignment&,
    const ExtendedVirtualDefaultedAssignment&>);
static_assert(std::is_assignable_v<
    ExtendedVirtualDefaultedAssignment&,
    ExtendedVirtualDefaultedAssignment&&>);
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
    if (move_destination.value.value != 73 ||
        inherited_fallback_copy_calls != 0 ||
        defaulted_move_assignment_calls != 1) return 6;

    DirectVirtualDefaultedAssignment virtual_copy_destination;
    DirectVirtualDefaultedAssignment virtual_copy_source;
    static_cast<TrackedVirtualAssignmentBase&>(
        virtual_copy_destination).value = 79;
    virtual_copy_destination.tail = 83;
    static_cast<TrackedVirtualAssignmentBase&>(virtual_copy_source).value = 89;
    virtual_copy_source.tail = 97;
    virtual_base_copy_assignment_calls = 0;
    virtual_copy_destination = virtual_copy_source;
    if (static_cast<TrackedVirtualAssignmentBase&>(
            virtual_copy_destination).value != 89 ||
        virtual_copy_destination.tail != 97 ||
        virtual_base_copy_assignment_calls != 1) return 7;

    DirectVirtualDefaultedAssignment virtual_move_destination;
    DirectVirtualDefaultedAssignment virtual_move_source;
    static_cast<TrackedVirtualAssignmentBase&>(
        virtual_move_destination).value = 101;
    virtual_move_destination.tail = 103;
    static_cast<TrackedVirtualAssignmentBase&>(virtual_move_source).value = 107;
    virtual_move_source.tail = 109;
    virtual_base_move_assignment_calls = 0;
    virtual_move_destination = static_cast<
        DirectVirtualDefaultedAssignment&&>(virtual_move_source);
    if (static_cast<TrackedVirtualAssignmentBase&>(
            virtual_move_destination).value != 107 ||
        virtual_move_destination.tail != 109 ||
        virtual_base_move_assignment_calls != 1) return 8;

    ExtendedVirtualDefaultedAssignment extended_copy_destination;
    ExtendedVirtualDefaultedAssignment extended_copy_source;
    static_cast<TrackedVirtualAssignmentBase&>(
        extended_copy_destination).value = 113;
    extended_copy_destination.tail = 127;
    extended_copy_destination.extra = 131;
    static_cast<TrackedVirtualAssignmentBase&>(
        extended_copy_source).value = 137;
    extended_copy_source.tail = 139;
    extended_copy_source.extra = 149;
    virtual_base_copy_assignment_calls = 0;
    extended_copy_destination = extended_copy_source;
    if (static_cast<TrackedVirtualAssignmentBase&>(
            extended_copy_destination).value != 137 ||
        extended_copy_destination.tail != 139 ||
        extended_copy_destination.extra != 149 ||
        virtual_base_copy_assignment_calls != 1) return 9;

    ExtendedVirtualDefaultedAssignment extended_move_destination;
    ExtendedVirtualDefaultedAssignment extended_move_source;
    static_cast<TrackedVirtualAssignmentBase&>(
        extended_move_destination).value = 151;
    extended_move_destination.tail = 157;
    extended_move_destination.extra = 163;
    static_cast<TrackedVirtualAssignmentBase&>(
        extended_move_source).value = 167;
    extended_move_source.tail = 173;
    extended_move_source.extra = 179;
    virtual_base_move_assignment_calls = 0;
    extended_move_destination = static_cast<
        ExtendedVirtualDefaultedAssignment&&>(extended_move_source);
    return static_cast<TrackedVirtualAssignmentBase&>(
               extended_move_destination).value == 167 &&
           extended_move_destination.tail == 173 &&
           extended_move_destination.extra == 179 &&
           virtual_base_move_assignment_calls == 1 ? 0 : 10;
}
