template <typename Type>
Type&& declval() noexcept;

template <typename Left, typename Right>
struct common_result {
    using type = decltype(false ? declval<Left(&)()>()()
                               : declval<Right(&)()>()());
};

template <typename Left, typename Right>
struct is_same {
    static constexpr bool value = false;
};

template <typename Type>
struct is_same<Type, Type> {
    static constexpr bool value = true;
};

static_assert(is_same<common_result<int, int>::type, int>::value,
              "decltype resolves calls through dependent function references");
