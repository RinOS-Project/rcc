namespace trailing_return_noexcept {

template<typename T>
struct add_rvalue_reference {
    using type = T&&;
};

template<typename T>
typename add_rvalue_reference<T>::type declval() noexcept;

template<typename T>
auto dereference(T&& value)
    noexcept(noexcept(*declval<T>())) -> decltype(*declval<T>());

template<typename T, typename U>
struct is_same {
    static constexpr bool value = false;
};

template<typename T>
struct is_same<T, T> {
    static constexpr bool value = true;
};

using DeclvalPointerResult = decltype(declval<int*>());
static_assert(is_same<DeclvalPointerResult, int*&&>::value,
              "declval preserves the requested rvalue-reference type");
using DereferenceResult = decltype(*declval<int*>());
static_assert(is_same<DereferenceResult, int&>::value,
              "dereferencing declval preserves the lvalue-reference type");
using TrailingReturnCallResult = decltype(dereference(declval<int*>()));
static_assert(is_same<TrailingReturnCallResult, int&>::value,
              "a dependent trailing return resolves after substitution");
static_assert(noexcept(dereference(declval<int*>())),
              "the instantiated dependent exception specification is retained");

}  // namespace trailing_return_noexcept
