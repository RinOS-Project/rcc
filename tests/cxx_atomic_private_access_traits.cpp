#if defined(__RCC__)
#define ATOMIC_IS_TRIVIALLY_COPYABLE(T) __is_trivially_copyable(T)
#define ATOMIC_IS_COPY_CONSTRUCTIBLE(T) __is_constructible(T, const T&)
#define ATOMIC_IS_MOVE_CONSTRUCTIBLE(T) __is_constructible(T, T&&)
#define ATOMIC_IS_COPY_ASSIGNABLE(T) __is_assignable(T&, const T&)
#define ATOMIC_IS_MOVE_ASSIGNABLE(T) __is_assignable(T&, T&&)
#else
#include <type_traits>
#define ATOMIC_IS_TRIVIALLY_COPYABLE(T) std::is_trivially_copyable_v<T>
#define ATOMIC_IS_COPY_CONSTRUCTIBLE(T) std::is_copy_constructible_v<T>
#define ATOMIC_IS_MOVE_CONSTRUCTIBLE(T) std::is_move_constructible_v<T>
#define ATOMIC_IS_COPY_ASSIGNABLE(T) std::is_copy_assignable_v<T>
#define ATOMIC_IS_MOVE_ASSIGNABLE(T) std::is_move_assignable_v<T>
#endif

struct AtomicPrivateCopyConstructorValue {
    AtomicPrivateCopyConstructorValue() = default;

private:
    AtomicPrivateCopyConstructorValue(
        const AtomicPrivateCopyConstructorValue&) = default;
};

struct AtomicProtectedCopyConstructorValue {
    AtomicProtectedCopyConstructorValue() = default;

protected:
    AtomicProtectedCopyConstructorValue(
        const AtomicProtectedCopyConstructorValue&) = default;
};

struct AtomicPrivateMoveConstructorValue {
    AtomicPrivateMoveConstructorValue() = default;
    AtomicPrivateMoveConstructorValue(
        const AtomicPrivateMoveConstructorValue&) = default;

private:
    AtomicPrivateMoveConstructorValue(
        AtomicPrivateMoveConstructorValue&&) = default;
};

struct AtomicProtectedMoveConstructorValue {
    AtomicProtectedMoveConstructorValue() = default;
    AtomicProtectedMoveConstructorValue(
        const AtomicProtectedMoveConstructorValue&) = default;

protected:
    AtomicProtectedMoveConstructorValue(
        AtomicProtectedMoveConstructorValue&&) = default;
};

struct AtomicPrivateCopyAssignmentValue {
    AtomicPrivateCopyAssignmentValue() = default;

private:
    AtomicPrivateCopyAssignmentValue& operator=(
        const AtomicPrivateCopyAssignmentValue&) = default;
};

struct AtomicPrivateMoveAssignmentValue {
    AtomicPrivateMoveAssignmentValue() = default;
    AtomicPrivateMoveAssignmentValue& operator=(
        const AtomicPrivateMoveAssignmentValue&) = default;

private:
    AtomicPrivateMoveAssignmentValue& operator=(
        AtomicPrivateMoveAssignmentValue&&) = default;
};

struct AtomicProtectedMoveAssignmentValue {
    AtomicProtectedMoveAssignmentValue() = default;
    AtomicProtectedMoveAssignmentValue& operator=(
        const AtomicProtectedMoveAssignmentValue&) = default;

protected:
    AtomicProtectedMoveAssignmentValue& operator=(
        AtomicProtectedMoveAssignmentValue&&) = default;
};

struct AtomicProtectedCopyAssignmentValue {
    AtomicProtectedCopyAssignmentValue() = default;

protected:
    AtomicProtectedCopyAssignmentValue& operator=(
        const AtomicProtectedCopyAssignmentValue&) = default;
};

#define ASSERT_ATOMIC_CONSTRUCTOR_ACCESS_TRAITS(T) \
    static_assert(ATOMIC_IS_TRIVIALLY_COPYABLE(T), \
                  #T " remains trivially copyable"); \
    static_assert(!ATOMIC_IS_COPY_CONSTRUCTIBLE(T), \
                  #T " is not copy constructible"); \
    static_assert(!ATOMIC_IS_MOVE_CONSTRUCTIBLE(T), \
                  #T " is not move constructible")

#define ASSERT_ATOMIC_ASSIGNMENT_ACCESS_TRAITS(T) \
    static_assert(ATOMIC_IS_TRIVIALLY_COPYABLE(T), \
                  #T " remains trivially copyable"); \
    static_assert(!ATOMIC_IS_COPY_ASSIGNABLE(T), \
                  #T " is not copy assignable"); \
    static_assert(!ATOMIC_IS_MOVE_ASSIGNABLE(T), \
                  #T " is not move assignable")

#define ASSERT_ATOMIC_MOVE_CONSTRUCTOR_ACCESS_TRAITS(T) \
    static_assert(ATOMIC_IS_TRIVIALLY_COPYABLE(T), \
                  #T " remains trivially copyable"); \
    static_assert(ATOMIC_IS_COPY_CONSTRUCTIBLE(T), \
                  #T " remains copy constructible"); \
    static_assert(!ATOMIC_IS_MOVE_CONSTRUCTIBLE(T), \
                  #T " is not move constructible")

#define ASSERT_ATOMIC_MOVE_ASSIGNMENT_ACCESS_TRAITS(T) \
    static_assert(ATOMIC_IS_TRIVIALLY_COPYABLE(T), \
                  #T " remains trivially copyable"); \
    static_assert(ATOMIC_IS_COPY_ASSIGNABLE(T), \
                  #T " remains copy assignable"); \
    static_assert(!ATOMIC_IS_MOVE_ASSIGNABLE(T), \
                  #T " is not move assignable")

ASSERT_ATOMIC_CONSTRUCTOR_ACCESS_TRAITS(AtomicPrivateCopyConstructorValue);
ASSERT_ATOMIC_CONSTRUCTOR_ACCESS_TRAITS(AtomicProtectedCopyConstructorValue);
ASSERT_ATOMIC_MOVE_CONSTRUCTOR_ACCESS_TRAITS(AtomicPrivateMoveConstructorValue);
ASSERT_ATOMIC_MOVE_CONSTRUCTOR_ACCESS_TRAITS(AtomicProtectedMoveConstructorValue);
ASSERT_ATOMIC_ASSIGNMENT_ACCESS_TRAITS(AtomicPrivateCopyAssignmentValue);
ASSERT_ATOMIC_ASSIGNMENT_ACCESS_TRAITS(AtomicProtectedCopyAssignmentValue);
ASSERT_ATOMIC_MOVE_ASSIGNMENT_ACCESS_TRAITS(AtomicPrivateMoveAssignmentValue);
ASSERT_ATOMIC_MOVE_ASSIGNMENT_ACCESS_TRAITS(AtomicProtectedMoveAssignmentValue);

#undef ASSERT_ATOMIC_CONSTRUCTOR_ACCESS_TRAITS
#undef ASSERT_ATOMIC_ASSIGNMENT_ACCESS_TRAITS
#undef ASSERT_ATOMIC_MOVE_CONSTRUCTOR_ACCESS_TRAITS
#undef ASSERT_ATOMIC_MOVE_ASSIGNMENT_ACCESS_TRAITS
#undef ATOMIC_IS_TRIVIALLY_COPYABLE
#undef ATOMIC_IS_COPY_CONSTRUCTIBLE
#undef ATOMIC_IS_MOVE_CONSTRUCTIBLE
#undef ATOMIC_IS_COPY_ASSIGNABLE
#undef ATOMIC_IS_MOVE_ASSIGNABLE
