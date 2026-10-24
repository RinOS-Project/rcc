namespace probe {

template<typename T, T Value>
struct integral_constant {
    static constexpr T value = Value;
};

template<typename T>
T&& declval() noexcept;

namespace detail {

template<typename To>
void test_convertible(To);

template<typename From, typename To, typename = void>
struct is_convertible_impl {
    static constexpr bool value = false;
};

template<typename From, typename To>
struct is_convertible_impl<
    From, To, decltype(detail::test_convertible<To>(declval<From>()))> {
    static constexpr bool value = true;
};

template<typename To>
void test_nothrow_convertible(To) noexcept;

template<typename From, typename To, typename = void>
struct is_nothrow_convertible_impl {
    static constexpr bool value = false;
};

template<typename From, typename To>
struct is_nothrow_convertible_impl<
    From, To,
    decltype(detail::test_convertible<To>(declval<From>()))>
    : integral_constant<bool,
        noexcept(detail::test_nothrow_convertible<To>(declval<From>()))> {};

}  // namespace detail

}  // namespace probe

template<typename T>
struct dependent_noexcept_value
    : probe::integral_constant<bool,
          noexcept(probe::detail::test_nothrow_convertible<T>(
              probe::declval<T>()))> {};

static_assert(probe::detail::is_convertible_impl<int, int>::value,
              "dependent decltype should select the viable specialization");
static_assert(noexcept(probe::detail::test_nothrow_convertible<int>(0)),
              "a declared noexcept function template call is non-throwing");
static_assert(dependent_noexcept_value<int>::value,
              "dependent noexcept template arguments are substituted");
static_assert(probe::detail::is_nothrow_convertible_impl<int, int>::value,
              "dependent noexcept should select the viable specialization");

