namespace probe {

template<typename T>
struct remove_const {
    using type = T;
};

template<typename T>
struct remove_const<const T> {
    using type = T;
};

template<typename T>
struct decay {
    using type = typename remove_const<T>::type;
};

template<typename T, typename U>
struct is_same {
    static constexpr bool value = false;
};

template<typename T>
struct is_same<T, T> {
    static constexpr bool value = true;
};

template<bool, typename T = void>
struct enable_if {};

template<typename T>
struct enable_if<true, T> {
    using type = T;
};

template<bool B, typename T = void>
using enable_if_t = typename enable_if<B, T>::type;

template<typename T, typename U, typename = void>
struct decay_impl {
    static constexpr bool value = false;
};

template<typename T, typename U>
struct decay_impl<T, U,
    enable_if_t<!is_same<T, typename decay<T>::type>::value ||
                !is_same<U, typename decay<U>::type>::value>> {
    static constexpr bool value = true;
};

}  // namespace probe

static_assert(!probe::is_same<const int, int>::value,
              "primary class template specialization should be selected");
static_assert(probe::is_same<typename probe::decay<const int>::type,
                             int>::value,
              "dependent nested alias should substitute");
static_assert(probe::is_same<
                  typename probe::enable_if<true, void>::type,
                  void>::value,
              "boolean class-template specialization should be selected");
static_assert(probe::is_same<probe::enable_if_t<true>, void>::value,
              "a concrete alias-template argument should be substituted");
static_assert(probe::is_same<
                  probe::enable_if_t<
                      !probe::is_same<const int,
                                      typename probe::decay<const int>::type>::value>,
                  void>::value,
              "alias-template boolean arguments should substitute");

static_assert(probe::decay_impl<const int, int>::value,
              "dependent alias arguments should remain substitutable");
static_assert(!probe::decay_impl<int, int>::value,
              "the false condition should leave the primary selected");
