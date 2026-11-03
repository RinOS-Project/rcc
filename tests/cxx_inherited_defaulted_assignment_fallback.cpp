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

#if defined(__RCC__)
static_assert(__is_assignable(
    InheritedFallbackDerived&, InheritedFallbackBase&&));
static_assert(__is_assignable(
    InheritedNontrivialFallbackDerived&,
    InheritedNontrivialFallbackBase&&));
static_assert(__is_assignable(
    DirectNontrivialDefaultedAssignment&,
    DirectNontrivialDefaultedAssignment&&));
#else
static_assert(std::is_assignable_v<
    InheritedFallbackDerived&, InheritedFallbackBase&&>);
static_assert(std::is_assignable_v<
    InheritedNontrivialFallbackDerived&,
    InheritedNontrivialFallbackBase&&>);
static_assert(std::is_assignable_v<
    DirectNontrivialDefaultedAssignment&,
    DirectNontrivialDefaultedAssignment&&>);
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
    return direct_destination.value.value == 41 &&
           inherited_fallback_copy_calls == 1 ? 0 : 4;
}
