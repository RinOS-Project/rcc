#include "../../../libs/libcxx/type_traits.h"

using NoexceptFunction = int(double) noexcept;
using NoexceptFunctionPointer = int (*)(double) noexcept;

template<typename T>
struct NoexceptFunctionProbe {
    static constexpr int value = -1;
};

template<typename R, typename... Args>
struct NoexceptFunctionProbe<R(Args...) noexcept> {
    static constexpr int value = sizeof...(Args);
};

static_assert(NoexceptFunctionProbe<NoexceptFunction>::value == 1,
              "noexcept function partial specialization matches");
static_assert(NoexceptFunctionProbe<int() noexcept>::value == 0,
              "noexcept function partial specialization accepts an empty pack");
static_assert(NoexceptFunctionProbe<int(double, char) noexcept>::value == 2,
              "noexcept function partial specialization expands a parameter pack");

static_assert(std::is_function<NoexceptFunction>::value,
              "noexcept function types remain function types");
static_assert(std::is_same<
                  std::add_pointer<NoexceptFunction>::type,
                  NoexceptFunctionPointer>::value,
              "adding a pointer preserves noexcept");
static_assert(std::is_same<
                  std::remove_reference<NoexceptFunction>::type,
                  NoexceptFunction>::value,
              "remove_reference preserves the function type");
static_assert(std::is_same<
                  std::__rcc_decay_impl<NoexceptFunction>::type,
                  NoexceptFunctionPointer>::value,
              "decay helper handles noexcept functions");
static_assert(std::is_same<
                  std::__rcc_decay_impl<NoexceptFunction>::Function,
                  NoexceptFunction>::value,
              "nested function alias preserves noexcept");
static_assert(std::is_pointer<
                  std::decay<NoexceptFunction>::type>::value,
              "decay produces a pointer");
static_assert(!std::is_function<
                  std::decay<NoexceptFunction>::type>::value,
              "decay result is not a function type");
