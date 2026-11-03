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

#if defined(__RCC__)
static_assert(__is_assignable(
    InheritedFallbackDerived&, InheritedFallbackBase&&));
#else
static_assert(std::is_assignable_v<
    InheritedFallbackDerived&, InheritedFallbackBase&&>);
#endif

int main() {
    InheritedFallbackDerived destination;
    InheritedFallbackBase source;
    destination.prefix = 7;
    destination.value.value = 11;
    source.value.value = 29;
    destination = static_cast<InheritedFallbackBase&&>(source);
    return destination.prefix == 7 && destination.value.value == 29
        ? 0 : 1;
}
