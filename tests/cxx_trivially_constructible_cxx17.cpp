#ifndef __RCC__
#include <type_traits>
#endif

struct Cxx17AggregateConstructible {
    int value;
};

union Cxx17UnionConstructible {
    int integer;
    double floating;
};

struct Cxx17NothrowCopyAssignment {
    Cxx17NothrowCopyAssignment& operator=(
        const Cxx17NothrowCopyAssignment&) noexcept { return *this; }
};

struct Cxx17ThrowingCopyAssignment {
    Cxx17ThrowingCopyAssignment& operator=(
        const Cxx17ThrowingCopyAssignment&) noexcept(false) { return *this; }
};

#ifdef __RCC__
static_assert(__is_trivially_constructible(Cxx17AggregateConstructible),
              "an aggregate remains default constructible in C++17");
static_assert(!__is_trivially_constructible(Cxx17AggregateConstructible, int),
              "parenthesized aggregate initialization is not available in C++17");
static_assert(!__is_trivially_constructible(int[2], int),
              "parenthesized array initialization is not available in C++17");
static_assert(__is_trivially_constructible(Cxx17UnionConstructible),
              "a scalar union remains default constructible in C++17");
static_assert(!__is_trivially_constructible(Cxx17UnionConstructible, int),
              "parenthesized union initialization is not available in C++17");
static_assert(__has_nothrow_assign(Cxx17NothrowCopyAssignment),
              "legacy nothrow assignment traits are available in C++17");
static_assert(!__has_nothrow_assign(Cxx17ThrowingCopyAssignment),
              "C++17 nothrow assignment checks exception specifications");
#else
static_assert(std::is_trivially_constructible_v<Cxx17AggregateConstructible>,
              "an aggregate remains default constructible in C++17");
static_assert(!std::is_trivially_constructible_v<Cxx17AggregateConstructible,
                                                 int>,
              "parenthesized aggregate initialization is not available in C++17");
static_assert(!std::is_trivially_constructible_v<int[2], int>,
              "parenthesized array initialization is not available in C++17");
static_assert(std::is_trivially_constructible_v<Cxx17UnionConstructible>,
              "a scalar union remains default constructible in C++17");
static_assert(!std::is_trivially_constructible_v<Cxx17UnionConstructible,
                                                 int>,
              "parenthesized union initialization is not available in C++17");
static_assert(__has_nothrow_assign(Cxx17NothrowCopyAssignment),
              "legacy nothrow assignment traits are available in C++17");
static_assert(!__has_nothrow_assign(Cxx17ThrowingCopyAssignment),
              "C++17 nothrow assignment checks exception specifications");
#endif
