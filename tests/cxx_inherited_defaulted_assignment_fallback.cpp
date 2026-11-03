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

struct InheritedNontrivialFallbackIntermediate :
    InheritedNontrivialFallbackBase {
    using InheritedNontrivialFallbackBase::operator=;
    int marker;
};

struct InheritedNontrivialFallbackIndirect : InheritedFallbackPrefix,
    InheritedNontrivialFallbackIntermediate {
    using InheritedNontrivialFallbackIntermediate::operator=;
    int tail;
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

static int virtual_using_member_copy_assignment_calls;
static int virtual_using_member_move_assignment_calls;

struct VirtualUsingAssignmentMember {
    int value;
    VirtualUsingAssignmentMember& operator=(
        const VirtualUsingAssignmentMember& source) {
        value = source.value;
        ++virtual_using_member_copy_assignment_calls;
        return *this;
    }
    VirtualUsingAssignmentMember& operator=(
        VirtualUsingAssignmentMember&& source) {
        value = source.value;
        ++virtual_using_member_move_assignment_calls;
        return *this;
    }
};

struct VirtualUsingAssignmentBase {
    VirtualUsingAssignmentMember member;
    VirtualUsingAssignmentBase& operator=(
        const VirtualUsingAssignmentBase&) = default;
    VirtualUsingAssignmentBase& operator=(
        VirtualUsingAssignmentBase&&) = default;
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

#if !defined(__RCC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvirtual-move-assign"
#endif
struct VirtualUsingDefaultedAssignment :
    virtual VirtualUsingAssignmentBase {
    using VirtualUsingAssignmentBase::operator=;
    int marker;
};
#if !defined(__RCC__)
#pragma GCC diagnostic pop
#endif

#if !defined(__RCC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvirtual-move-assign"
#endif
struct ExtendedVirtualUsingAssignment :
    VirtualUsingDefaultedAssignment {
    using VirtualUsingDefaultedAssignment::operator=;
    long long padding[8];
};
#if !defined(__RCC__)
#pragma GCC diagnostic pop
#endif

struct NestedVirtualUsingOwner : VirtualUsingAssignmentBase {
    using VirtualUsingAssignmentBase::operator=;
    int nested_marker;
};

#if !defined(__RCC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvirtual-move-assign"
#endif
struct VirtualNestedUsingLayer : virtual NestedVirtualUsingOwner {
    using NestedVirtualUsingOwner::operator=;
    int layer_marker;
};
struct ExtendedVirtualNestedUsingAssignment : VirtualNestedUsingLayer {
    using VirtualNestedUsingLayer::operator=;
    long long padding[8];
};
#if !defined(__RCC__)
#pragma GCC diagnostic pop
#endif

struct IntegerInheritedAssignmentBase {
    int value;
    int calls;
    IntegerInheritedAssignmentBase& operator=(int source) {
        value = source;
        ++calls;
        return *this;
    }
};

struct FloatingInheritedAssignmentBase {
    double value;
    int calls;
    FloatingInheritedAssignmentBase& operator=(double source) {
        value = source;
        ++calls;
        return *this;
    }
};

struct MultipleInheritedAssignmentOverloads : IntegerInheritedAssignmentBase,
    FloatingInheritedAssignmentBase {
    using IntegerInheritedAssignmentBase::operator=;
    using FloatingInheritedAssignmentBase::operator=;
    int marker;
};

struct AmbiguousIntegerAssignmentBaseA {
    AmbiguousIntegerAssignmentBaseA& operator=(int);
};

struct AmbiguousIntegerAssignmentBaseB {
    AmbiguousIntegerAssignmentBaseB& operator=(int);
};

struct AmbiguousMultipleInheritedAssignmentOverloads :
    AmbiguousIntegerAssignmentBaseA, AmbiguousIntegerAssignmentBaseB {
    using AmbiguousIntegerAssignmentBaseA::operator=;
    using AmbiguousIntegerAssignmentBaseB::operator=;
};

struct CvInheritedAssignmentBase {
    int nonconst_calls;
    int const_calls;
    CvInheritedAssignmentBase& operator=(CvInheritedAssignmentBase&) {
        ++nonconst_calls;
        return *this;
    }
    CvInheritedAssignmentBase& operator=(
        const CvInheritedAssignmentBase&) {
        ++const_calls;
        return *this;
    }
};

struct CvInheritedAssignmentDerived : CvInheritedAssignmentBase {
    using CvInheritedAssignmentBase::operator=;
};

static int array_element_copy_assignment_calls;
static int array_element_move_assignment_calls;
static int array_element_assignment_order;

struct TrackedArrayAssignmentElement {
    int value;
    TrackedArrayAssignmentElement& operator=(
        const TrackedArrayAssignmentElement& source) {
        value = source.value;
        ++array_element_copy_assignment_calls;
        array_element_assignment_order =
            array_element_assignment_order * 10 + value;
        return *this;
    }
    TrackedArrayAssignmentElement& operator=(
        TrackedArrayAssignmentElement&& source) {
        value = source.value;
        ++array_element_move_assignment_calls;
        array_element_assignment_order =
            array_element_assignment_order * 10 + value;
        return *this;
    }
};

struct DefaultedArrayMemberAssignment {
    TrackedArrayAssignmentElement values[2][2];
    int tail;
    DefaultedArrayMemberAssignment& operator=(
        const DefaultedArrayMemberAssignment&) = default;
    DefaultedArrayMemberAssignment& operator=(
        DefaultedArrayMemberAssignment&&) = default;
};

static int bitfield_payload_copy_assignment_calls;
static int bitfield_payload_move_assignment_calls;

struct TrackedBitfieldAssignmentPayload {
    int value;
    TrackedBitfieldAssignmentPayload& operator=(
        const TrackedBitfieldAssignmentPayload& source) {
        value = source.value;
        ++bitfield_payload_copy_assignment_calls;
        return *this;
    }
    TrackedBitfieldAssignmentPayload& operator=(
        TrackedBitfieldAssignmentPayload&& source) {
        value = source.value;
        ++bitfield_payload_move_assignment_calls;
        return *this;
    }
};

struct DefaultedBitfieldAssignment {
    unsigned int low : 3;
    unsigned int middle : 5;
    unsigned int high : 4;
    TrackedBitfieldAssignmentPayload payload;
    DefaultedBitfieldAssignment& operator=(
        const DefaultedBitfieldAssignment&) = default;
    DefaultedBitfieldAssignment& operator=(
        DefaultedBitfieldAssignment&&) = default;
};

#if defined(__RCC__)
static_assert(__is_assignable(
    InheritedFallbackDerived&, InheritedFallbackBase&&));
static_assert(__is_assignable(
    InheritedNontrivialFallbackDerived&,
    InheritedNontrivialFallbackBase&&));
static_assert(__is_assignable(
    InheritedNontrivialFallbackIndirect&,
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
    VirtualUsingDefaultedAssignment&,
    const VirtualUsingAssignmentBase&));
static_assert(__is_assignable(
    VirtualUsingDefaultedAssignment&,
    VirtualUsingAssignmentBase&&));
static_assert(__is_assignable(
    ExtendedVirtualUsingAssignment&,
    const VirtualUsingAssignmentBase&));
static_assert(__is_assignable(
    ExtendedVirtualUsingAssignment&,
    VirtualUsingAssignmentBase&&));
static_assert(__is_assignable(
    ExtendedVirtualNestedUsingAssignment&,
    const VirtualUsingAssignmentBase&));
static_assert(__is_assignable(
    ExtendedVirtualNestedUsingAssignment&,
    VirtualUsingAssignmentBase&&));
static_assert(__is_assignable(MultipleInheritedAssignmentOverloads&, int));
static_assert(__is_assignable(MultipleInheritedAssignmentOverloads&, double));
static_assert(!__is_assignable(
    AmbiguousMultipleInheritedAssignmentOverloads&, int));
static_assert(__is_assignable(CvInheritedAssignmentDerived&,
                              CvInheritedAssignmentBase&));
static_assert(__is_assignable(CvInheritedAssignmentDerived&,
                              const CvInheritedAssignmentBase&));
static_assert(__is_assignable(
    ExtendedVirtualDefaultedAssignment&,
    const ExtendedVirtualDefaultedAssignment&));
static_assert(__is_assignable(
    ExtendedVirtualDefaultedAssignment&,
    ExtendedVirtualDefaultedAssignment&&));
static_assert(__is_assignable(
    DefaultedArrayMemberAssignment&,
    const DefaultedArrayMemberAssignment&));
static_assert(__is_assignable(
    DefaultedArrayMemberAssignment&,
    DefaultedArrayMemberAssignment&&));
static_assert(__is_assignable(
    DefaultedBitfieldAssignment&,
    const DefaultedBitfieldAssignment&));
static_assert(__is_assignable(
    DefaultedBitfieldAssignment&,
    DefaultedBitfieldAssignment&&));
#else
static_assert(std::is_assignable_v<
    InheritedFallbackDerived&, InheritedFallbackBase&&>);
static_assert(std::is_assignable_v<
    InheritedNontrivialFallbackDerived&,
    InheritedNontrivialFallbackBase&&>);
static_assert(std::is_assignable_v<
    InheritedNontrivialFallbackIndirect&,
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
    VirtualUsingDefaultedAssignment&,
    const VirtualUsingAssignmentBase&>);
static_assert(std::is_assignable_v<
    VirtualUsingDefaultedAssignment&,
    VirtualUsingAssignmentBase&&>);
static_assert(std::is_assignable_v<
    ExtendedVirtualUsingAssignment&,
    const VirtualUsingAssignmentBase&>);
static_assert(std::is_assignable_v<
    ExtendedVirtualUsingAssignment&,
    VirtualUsingAssignmentBase&&>);
static_assert(std::is_assignable_v<
    ExtendedVirtualNestedUsingAssignment&,
    const VirtualUsingAssignmentBase&>);
static_assert(std::is_assignable_v<
    ExtendedVirtualNestedUsingAssignment&,
    VirtualUsingAssignmentBase&&>);
static_assert(std::is_assignable_v<
    MultipleInheritedAssignmentOverloads&, int>);
static_assert(std::is_assignable_v<
    MultipleInheritedAssignmentOverloads&, double>);
static_assert(!std::is_assignable_v<
    AmbiguousMultipleInheritedAssignmentOverloads&, int>);
static_assert(std::is_assignable_v<CvInheritedAssignmentDerived&,
                                   CvInheritedAssignmentBase&>);
static_assert(std::is_assignable_v<CvInheritedAssignmentDerived&,
                                   const CvInheritedAssignmentBase&>);
static_assert(std::is_assignable_v<
    ExtendedVirtualDefaultedAssignment&,
    const ExtendedVirtualDefaultedAssignment&>);
static_assert(std::is_assignable_v<
    ExtendedVirtualDefaultedAssignment&,
    ExtendedVirtualDefaultedAssignment&&>);
static_assert(std::is_assignable_v<
    DefaultedArrayMemberAssignment&,
    const DefaultedArrayMemberAssignment&>);
static_assert(std::is_assignable_v<
    DefaultedArrayMemberAssignment&,
    DefaultedArrayMemberAssignment&&>);
static_assert(std::is_assignable_v<
    DefaultedBitfieldAssignment&,
    const DefaultedBitfieldAssignment&>);
static_assert(std::is_assignable_v<
    DefaultedBitfieldAssignment&,
    DefaultedBitfieldAssignment&&>);
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

    InheritedNontrivialFallbackIndirect indirect_nontrivial_destination;
    InheritedNontrivialFallbackBase indirect_nontrivial_source;
    indirect_nontrivial_destination.prefix = 5;
    indirect_nontrivial_destination.marker = 7;
    indirect_nontrivial_destination.tail = 11;
    indirect_nontrivial_destination.value.value = 13;
    indirect_nontrivial_source.value.value = 17;
    inherited_fallback_copy_calls = 0;
    indirect_nontrivial_destination = static_cast<
        InheritedNontrivialFallbackBase&&>(indirect_nontrivial_source);
    if (indirect_nontrivial_destination.prefix != 5 ||
        indirect_nontrivial_destination.value.value != 17 ||
        indirect_nontrivial_destination.marker != 7 ||
        indirect_nontrivial_destination.tail != 11 ||
        inherited_fallback_copy_calls != 1) return 21;

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

    VirtualUsingDefaultedAssignment virtual_using_copy_destination;
    VirtualUsingAssignmentBase virtual_using_copy_source;
    static_cast<VirtualUsingAssignmentBase&>(
        virtual_using_copy_destination).member.value = 191;
    virtual_using_copy_destination.marker = 193;
    virtual_using_copy_source.member.value = 197;
    virtual_using_member_copy_assignment_calls = 0;
    virtual_using_copy_destination = virtual_using_copy_source;
    if (static_cast<VirtualUsingAssignmentBase&>(
            virtual_using_copy_destination).member.value != 197 ||
        virtual_using_copy_destination.marker != 193 ||
        virtual_using_member_copy_assignment_calls != 1) return 15;

    VirtualUsingDefaultedAssignment virtual_using_move_destination;
    VirtualUsingAssignmentBase virtual_using_move_source;
    static_cast<VirtualUsingAssignmentBase&>(
        virtual_using_move_destination).member.value = 199;
    virtual_using_move_destination.marker = 211;
    virtual_using_move_source.member.value = 223;
    virtual_using_member_move_assignment_calls = 0;
    virtual_using_move_destination = static_cast<
        VirtualUsingAssignmentBase&&>(virtual_using_move_source);
    if (static_cast<VirtualUsingAssignmentBase&>(
            virtual_using_move_destination).member.value != 223 ||
        virtual_using_move_destination.marker != 211 ||
        virtual_using_member_move_assignment_calls != 1) return 16;

    ExtendedVirtualUsingAssignment extended_using_destination;
    VirtualUsingAssignmentBase extended_using_copy_source;
    static_cast<VirtualUsingAssignmentBase&>(
        extended_using_destination).member.value = 269;
    static_cast<VirtualUsingAssignmentBase&>(
        extended_using_copy_source).member.value = 271;
    extended_using_destination.marker = 277;
    extended_using_destination.padding[0] = 281;
    virtual_using_member_copy_assignment_calls = 0;
    static_cast<VirtualUsingDefaultedAssignment&>(
        extended_using_destination) = extended_using_copy_source;
    if (static_cast<VirtualUsingAssignmentBase&>(
            extended_using_destination).member.value != 271 ||
        extended_using_destination.marker != 277 ||
        extended_using_destination.padding[0] != 281 ||
        virtual_using_member_copy_assignment_calls != 1) return 17;

    VirtualUsingAssignmentBase extended_using_move_source;
    static_cast<VirtualUsingAssignmentBase&>(
        extended_using_destination).member.value = 283;
    static_cast<VirtualUsingAssignmentBase&>(
        extended_using_move_source).member.value = 293;
    virtual_using_member_move_assignment_calls = 0;
    static_cast<VirtualUsingDefaultedAssignment&>(
        extended_using_destination) = static_cast<
            VirtualUsingAssignmentBase&&>(extended_using_move_source);
    if (static_cast<VirtualUsingAssignmentBase&>(
            extended_using_destination).member.value != 293 ||
        extended_using_destination.marker != 277 ||
        extended_using_destination.padding[0] != 281 ||
        virtual_using_member_move_assignment_calls != 1) return 18;

    ExtendedVirtualUsingAssignment indirect_using_destination;
    VirtualUsingAssignmentBase indirect_using_copy_source;
    static_cast<VirtualUsingAssignmentBase&>(
        indirect_using_destination).member.value = 307;
    indirect_using_destination.marker = 311;
    indirect_using_destination.padding[0] = 313;
    indirect_using_copy_source.member.value = 317;
    virtual_using_member_copy_assignment_calls = 0;
    indirect_using_destination = indirect_using_copy_source;
    if (static_cast<VirtualUsingAssignmentBase&>(
            indirect_using_destination).member.value != 317 ||
        indirect_using_destination.marker != 311 ||
        indirect_using_destination.padding[0] != 313 ||
        virtual_using_member_copy_assignment_calls != 1) return 19;

    VirtualUsingAssignmentBase indirect_using_move_source;
    static_cast<VirtualUsingAssignmentBase&>(
        indirect_using_destination).member.value = 331;
    indirect_using_move_source.member.value = 337;
    virtual_using_member_move_assignment_calls = 0;
    indirect_using_destination = static_cast<
        VirtualUsingAssignmentBase&&>(indirect_using_move_source);
    if (static_cast<VirtualUsingAssignmentBase&>(
            indirect_using_destination).member.value != 337 ||
        indirect_using_destination.marker != 311 ||
        indirect_using_destination.padding[0] != 313 ||
        virtual_using_member_move_assignment_calls != 1) return 20;

    ExtendedVirtualNestedUsingAssignment nested_virtual_destination;
    VirtualUsingAssignmentBase nested_virtual_copy_source;
    static_cast<VirtualUsingAssignmentBase&>(
        nested_virtual_destination).member.value = 347;
    static_cast<NestedVirtualUsingOwner&>(
        nested_virtual_destination).nested_marker = 349;
    nested_virtual_destination.layer_marker = 353;
    nested_virtual_destination.padding[0] = 359;
    nested_virtual_copy_source.member.value = 367;
    virtual_using_member_copy_assignment_calls = 0;
    nested_virtual_destination = nested_virtual_copy_source;
    if (static_cast<VirtualUsingAssignmentBase&>(
            nested_virtual_destination).member.value != 367 ||
        static_cast<NestedVirtualUsingOwner&>(
            nested_virtual_destination).nested_marker != 349 ||
        nested_virtual_destination.layer_marker != 353 ||
        nested_virtual_destination.padding[0] != 359 ||
        virtual_using_member_copy_assignment_calls != 1) return 22;

    VirtualUsingAssignmentBase nested_virtual_move_source;
    static_cast<VirtualUsingAssignmentBase&>(
        nested_virtual_destination).member.value = 373;
    nested_virtual_move_source.member.value = 379;
    virtual_using_member_move_assignment_calls = 0;
    nested_virtual_destination = static_cast<
        VirtualUsingAssignmentBase&&>(nested_virtual_move_source);
    if (static_cast<VirtualUsingAssignmentBase&>(
            nested_virtual_destination).member.value != 379 ||
        static_cast<NestedVirtualUsingOwner&>(
            nested_virtual_destination).nested_marker != 349 ||
        nested_virtual_destination.layer_marker != 353 ||
        nested_virtual_destination.padding[0] != 359 ||
        virtual_using_member_move_assignment_calls != 1) return 23;

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
    if (static_cast<TrackedVirtualAssignmentBase&>(
            extended_move_destination).value != 167 ||
        extended_move_destination.tail != 173 ||
        extended_move_destination.extra != 179 ||
        virtual_base_move_assignment_calls != 1) return 10;

    DefaultedArrayMemberAssignment array_copy_destination;
    DefaultedArrayMemberAssignment array_copy_source;
    array_copy_source.values[0][0].value = 1;
    array_copy_source.values[0][1].value = 2;
    array_copy_source.values[1][0].value = 3;
    array_copy_source.values[1][1].value = 4;
    array_copy_source.tail = 181;
    array_element_copy_assignment_calls = 0;
    array_element_move_assignment_calls = 0;
    array_element_assignment_order = 0;
    array_copy_destination = array_copy_source;
    if (array_copy_destination.values[0][0].value != 1 ||
        array_copy_destination.values[0][1].value != 2 ||
        array_copy_destination.values[1][0].value != 3 ||
        array_copy_destination.values[1][1].value != 4 ||
        array_copy_destination.tail != 181 ||
        array_element_copy_assignment_calls != 4 ||
        array_element_move_assignment_calls != 0 ||
        array_element_assignment_order != 1234) return 11;

    DefaultedArrayMemberAssignment array_move_destination;
    DefaultedArrayMemberAssignment array_move_source;
    array_move_source.values[0][0].value = 5;
    array_move_source.values[0][1].value = 6;
    array_move_source.values[1][0].value = 7;
    array_move_source.values[1][1].value = 8;
    array_move_source.tail = 191;
    array_element_copy_assignment_calls = 0;
    array_element_move_assignment_calls = 0;
    array_element_assignment_order = 0;
    array_move_destination = static_cast<
        DefaultedArrayMemberAssignment&&>(array_move_source);
    if (array_move_destination.values[0][0].value != 5 ||
        array_move_destination.values[0][1].value != 6 ||
        array_move_destination.values[1][0].value != 7 ||
        array_move_destination.values[1][1].value != 8 ||
        array_move_destination.tail != 191 ||
        array_element_copy_assignment_calls != 0 ||
        array_element_move_assignment_calls != 4 ||
        array_element_assignment_order != 5678) return 12;

    DefaultedBitfieldAssignment bitfield_copy_destination;
    DefaultedBitfieldAssignment bitfield_copy_source;
    bitfield_copy_destination.low = 1;
    bitfield_copy_destination.middle = 2;
    bitfield_copy_destination.high = 3;
    bitfield_copy_destination.payload.value = 193;
    bitfield_copy_source.low = 5;
    bitfield_copy_source.middle = 17;
    bitfield_copy_source.high = 9;
    bitfield_copy_source.payload.value = 197;
    bitfield_payload_copy_assignment_calls = 0;
    bitfield_copy_destination = bitfield_copy_source;
    if (bitfield_copy_destination.low != 5 ||
        bitfield_copy_destination.middle != 17 ||
        bitfield_copy_destination.high != 9 ||
        bitfield_copy_destination.payload.value != 197 ||
        bitfield_payload_copy_assignment_calls != 1) return 13;

    DefaultedBitfieldAssignment bitfield_move_destination;
    DefaultedBitfieldAssignment bitfield_move_source;
    bitfield_move_destination.low = 2;
    bitfield_move_destination.middle = 4;
    bitfield_move_destination.high = 6;
    bitfield_move_destination.payload.value = 199;
    bitfield_move_source.low = 7;
    bitfield_move_source.middle = 23;
    bitfield_move_source.high = 11;
    bitfield_move_source.payload.value = 211;
    bitfield_payload_copy_assignment_calls = 0;
    bitfield_payload_move_assignment_calls = 0;
    bitfield_move_destination = static_cast<
        DefaultedBitfieldAssignment&&>(bitfield_move_source);
    if (bitfield_move_destination.low != 7 ||
        bitfield_move_destination.middle != 23 ||
        bitfield_move_destination.high != 11 ||
        bitfield_move_destination.payload.value != 211 ||
        bitfield_payload_copy_assignment_calls != 0 ||
        bitfield_payload_move_assignment_calls != 1) return 14;

    MultipleInheritedAssignmentOverloads multiple_overload_destination;
    multiple_overload_destination.marker = 223;
    multiple_overload_destination = 227;
    if (static_cast<IntegerInheritedAssignmentBase&>(
            multiple_overload_destination).value != 227 ||
        static_cast<IntegerInheritedAssignmentBase&>(
            multiple_overload_destination).calls != 1 ||
        static_cast<FloatingInheritedAssignmentBase&>(
            multiple_overload_destination).calls != 0 ||
        multiple_overload_destination.marker != 223) return 24;
    multiple_overload_destination = 229.5;
    if (static_cast<IntegerInheritedAssignmentBase&>(
            multiple_overload_destination).value != 227 ||
        static_cast<IntegerInheritedAssignmentBase&>(
            multiple_overload_destination).calls != 1 ||
        static_cast<FloatingInheritedAssignmentBase&>(
            multiple_overload_destination).value != 229.5 ||
        static_cast<FloatingInheritedAssignmentBase&>(
            multiple_overload_destination).calls != 1 ||
        multiple_overload_destination.marker != 223) return 25;

    CvInheritedAssignmentDerived cv_overload_destination{};
    CvInheritedAssignmentBase nonconst_cv_source{};
    const CvInheritedAssignmentBase const_cv_source{};
    cv_overload_destination = nonconst_cv_source;
    if (static_cast<CvInheritedAssignmentBase&>(
            cv_overload_destination).nonconst_calls != 1 ||
        static_cast<CvInheritedAssignmentBase&>(
            cv_overload_destination).const_calls != 0) return 26;
    cv_overload_destination = const_cv_source;
    return static_cast<CvInheritedAssignmentBase&>(
               cv_overload_destination).nonconst_calls == 1 &&
           static_cast<CvInheritedAssignmentBase&>(
               cv_overload_destination).const_calls == 1 ? 0 : 27;
}
