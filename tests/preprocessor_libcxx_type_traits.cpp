#include "../../../libs/libcxx/type_traits.h"

int preprocessor_libcxx_type_traits() {
    return 0;
}

static_assert(std::is_same<decltype(std::declval<void>()), void>::value,
              "declval<void> returns void");
static_assert(std::is_same<std::decay<int[3]>::type, int*>::value,
              "decay converts arrays to element pointers");
static_assert(std::is_same<std::decay<int(double) noexcept>::type,
                           int (*)(double) noexcept>::value,
              "C++17 decay preserves noexcept function types");
