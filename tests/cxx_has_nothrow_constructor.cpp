int potentially_throwing_integer();
int nonthrowing_integer() noexcept;

struct NothrowDefaultValue {
    NothrowDefaultValue() noexcept {}
};

struct ThrowingDefaultValue {
    ThrowingDefaultValue() {}
};

struct DeletedDefaultConstructorValue {
    DeletedDefaultConstructorValue() = delete;
};

struct PrivateDefaultConstructorValue {
private:
    PrivateDefaultConstructorValue() = default;
};

struct PrivateNothrowDefaultConstructorValue {
private:
    PrivateNothrowDefaultConstructorValue() noexcept {}
};

struct ThrowingDestructorValue {
    ThrowingDestructorValue() noexcept {}
    ~ThrowingDestructorValue() noexcept(false) {}
};

struct ContainsThrowingDestructorValue {
    ThrowingDestructorValue value;
};

struct NothrowDefaultArgumentValue {
    NothrowDefaultArgumentValue(
        int value = nonthrowing_integer()) noexcept {
        (void)value;
    }
};

struct ThrowingDefaultArgumentValue {
    ThrowingDefaultArgumentValue(
        int value = potentially_throwing_integer()) noexcept {
        (void)value;
    }
};

struct NothrowDefaultMemberInitializer {
    int value = nonthrowing_integer();
};

struct ThrowingDefaultMemberInitializer {
    int value = potentially_throwing_integer();
};

template <typename T>
struct NothrowConstructorTraitProbe {
    static constexpr bool value = __has_nothrow_constructor(T);
};

static_assert(__has_nothrow_constructor(int),
              "scalar default construction is non-throwing");
static_assert(__has_nothrow_constructor(NothrowDefaultValue),
              "a noexcept default constructor is non-throwing");
static_assert(!__has_nothrow_constructor(ThrowingDefaultValue),
              "a potentially-throwing default constructor is not nothrow");
static_assert(__has_nothrow_constructor(ThrowingDestructorValue),
              "the legacy trait does not include destruction of a temporary");
static_assert(__has_nothrow_constructor(ContainsThrowingDestructorValue),
              "member destruction does not make default construction throw");
static_assert(__has_nothrow_constructor(NothrowDefaultValue[2]) &&
                  !__has_nothrow_constructor(ThrowingDefaultValue[]),
              "default-constructor exception status propagates through arrays");
static_assert(!__has_nothrow_constructor(void) &&
                  !__has_nothrow_constructor(int&),
              "void and references have no default constructor");
static_assert(__has_nothrow_constructor(DeletedDefaultConstructorValue),
              "the legacy trait reports a trivial deleted default constructor");
static_assert(__has_nothrow_constructor(PrivateDefaultConstructorValue),
              "the legacy trait reports a private trivial default constructor");
static_assert(__has_nothrow_constructor(PrivateNothrowDefaultConstructorValue),
              "the legacy trait reports a private noexcept default constructor");
static_assert(__has_nothrow_constructor(NothrowDefaultArgumentValue) &&
                  __has_nothrow_constructor(ThrowingDefaultArgumentValue),
              "the legacy trait checks constructor noexcept, not default arguments");
static_assert(__has_nothrow_constructor(NothrowDefaultMemberInitializer) &&
                  !__has_nothrow_constructor(ThrowingDefaultMemberInitializer),
              "default member initializer exceptions contribute to the trait");
static_assert(NothrowConstructorTraitProbe<NothrowDefaultValue>::value &&
                  !NothrowConstructorTraitProbe<ThrowingDefaultValue>::value,
              "dependent operands are evaluated after type substitution");
