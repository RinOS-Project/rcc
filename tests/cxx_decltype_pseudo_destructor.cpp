template<typename T>
T&& declval() noexcept;

using declval_result = decltype(declval<int>());

template<typename T, typename = void>
struct is_destructible_probe {
    static constexpr bool value = false;
};

template<typename T>
struct is_destructible_probe<T, decltype(declval<T&>().~T())> {
    static constexpr bool value = true;
};

struct complete_type {};
struct deleted_destructor {
    ~deleted_destructor() = delete;
};
struct private_destructor {
private:
    ~private_destructor() = default;
};

static_assert(is_destructible_probe<complete_type>::value,
              "pseudo-destructor expression should be valid");
static_assert(!is_destructible_probe<deleted_destructor>::value,
              "a deleted destructor should fail substitution");
static_assert(!is_destructible_probe<private_destructor>::value,
              "an inaccessible destructor should fail substitution");
