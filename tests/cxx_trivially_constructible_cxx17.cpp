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
#endif
