#if defined(__RCC__)
#define ATOMIC_IS_TRIVIALLY_COPYABLE(T) __is_trivially_copyable(T)
#define ATOMIC_IS_COPY_CONSTRUCTIBLE(T) __is_constructible(T, const T&)
#define ATOMIC_IS_MOVE_CONSTRUCTIBLE(T) __is_constructible(T, T&&)
#define ATOMIC_IS_COPY_ASSIGNABLE(T) __is_assignable(T&, const T&)
#define ATOMIC_IS_MOVE_ASSIGNABLE(T) __is_assignable(T&, T&&)
#else
#include <atomic>
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

struct AtomicPrivateSubobjectCopyConstructor {
    AtomicPrivateSubobjectCopyConstructor() = default;
    AtomicPrivateSubobjectCopyConstructor(
        AtomicPrivateSubobjectCopyConstructor&&) = default;
    AtomicPrivateSubobjectCopyConstructor& operator=(
        const AtomicPrivateSubobjectCopyConstructor&) = default;
    AtomicPrivateSubobjectCopyConstructor& operator=(
        AtomicPrivateSubobjectCopyConstructor&&) = default;

private:
    AtomicPrivateSubobjectCopyConstructor(
        const AtomicPrivateSubobjectCopyConstructor&) = default;
};

struct AtomicPrivateSubobjectCopyConstructorOwner {
    AtomicPrivateSubobjectCopyConstructor value;
};

struct AtomicPrivateSubobjectCopyAssignment {
    AtomicPrivateSubobjectCopyAssignment() = default;
    AtomicPrivateSubobjectCopyAssignment(
        const AtomicPrivateSubobjectCopyAssignment&) = default;
    AtomicPrivateSubobjectCopyAssignment(
        AtomicPrivateSubobjectCopyAssignment&&) = default;
    AtomicPrivateSubobjectCopyAssignment& operator=(
        AtomicPrivateSubobjectCopyAssignment&&) = default;

private:
    AtomicPrivateSubobjectCopyAssignment& operator=(
        const AtomicPrivateSubobjectCopyAssignment&) = default;
};

struct AtomicPrivateSubobjectCopyAssignmentOwner {
    AtomicPrivateSubobjectCopyAssignment value;
};

struct AtomicFriendSubobjectOwner;

struct AtomicPrivateFriendSubobject {
    AtomicPrivateFriendSubobject() = default;
    AtomicPrivateFriendSubobject(AtomicPrivateFriendSubobject&&) = default;
    AtomicPrivateFriendSubobject& operator=(
        AtomicPrivateFriendSubobject&&) = default;

private:
    AtomicPrivateFriendSubobject(
        const AtomicPrivateFriendSubobject&) = default;
    AtomicPrivateFriendSubobject& operator=(
        const AtomicPrivateFriendSubobject&) = default;
    friend struct AtomicFriendSubobjectOwner;
};

struct AtomicFriendSubobjectOwner {
    AtomicPrivateFriendSubobject value;
};

struct AtomicPrivateCopyBaseOwner;

struct AtomicPrivateCopyBase {
    AtomicPrivateCopyBase() = default;
    AtomicPrivateCopyBase(AtomicPrivateCopyBase&&) = default;
    AtomicPrivateCopyBase& operator=(AtomicPrivateCopyBase&&) = default;

private:
    AtomicPrivateCopyBase(const AtomicPrivateCopyBase&) = default;
    AtomicPrivateCopyBase& operator=(const AtomicPrivateCopyBase&) = default;
    friend struct AtomicPrivateCopyBaseOwner;
};

struct AtomicPrivateCopyBaseOwner : AtomicPrivateCopyBase {};

struct AtomicPrivateBaseOwner : AtomicPrivateCopyBase {};

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

static_assert(ATOMIC_IS_TRIVIALLY_COPYABLE(
                  AtomicPrivateSubobjectCopyConstructorOwner),
              "private member copy construction leaves Owner trivially copyable");
static_assert(!ATOMIC_IS_COPY_CONSTRUCTIBLE(
                  AtomicPrivateSubobjectCopyConstructorOwner),
              "a private member copy constructor deletes Owner copy construction");
static_assert(ATOMIC_IS_COPY_ASSIGNABLE(
                  AtomicPrivateSubobjectCopyConstructorOwner),
              "Owner copy assignment remains available");
static_assert(ATOMIC_IS_TRIVIALLY_COPYABLE(
                  AtomicPrivateSubobjectCopyAssignmentOwner),
              "private member copy assignment leaves Owner trivially copyable");
static_assert(ATOMIC_IS_COPY_CONSTRUCTIBLE(
                  AtomicPrivateSubobjectCopyAssignmentOwner),
              "Owner copy construction remains available");
static_assert(!ATOMIC_IS_COPY_ASSIGNABLE(
                  AtomicPrivateSubobjectCopyAssignmentOwner),
              "a private member copy assignment deletes Owner copy assignment");
static_assert(ATOMIC_IS_TRIVIALLY_COPYABLE(AtomicFriendSubobjectOwner),
              "friend access preserves trivial copyability");
static_assert(ATOMIC_IS_COPY_CONSTRUCTIBLE(AtomicFriendSubobjectOwner),
              "the friend Owner can copy its private subobject");
static_assert(ATOMIC_IS_COPY_ASSIGNABLE(AtomicFriendSubobjectOwner),
              "the friend Owner can copy-assign its private subobject");
static_assert(ATOMIC_IS_TRIVIALLY_COPYABLE(AtomicPrivateBaseOwner),
              "a private base operation leaves the derived type trivially copyable");
static_assert(!ATOMIC_IS_COPY_CONSTRUCTIBLE(AtomicPrivateBaseOwner),
              "a private base copy constructor deletes derived copy construction");
static_assert(!ATOMIC_IS_COPY_ASSIGNABLE(AtomicPrivateBaseOwner),
              "a private base copy assignment deletes derived copy assignment");
static_assert(ATOMIC_IS_TRIVIALLY_COPYABLE(AtomicPrivateCopyBaseOwner),
              "friend access preserves base-derived trivial copyability");
static_assert(ATOMIC_IS_COPY_CONSTRUCTIBLE(AtomicPrivateCopyBaseOwner),
              "the friend derived type can copy its private base");
static_assert(ATOMIC_IS_COPY_ASSIGNABLE(AtomicPrivateCopyBaseOwner),
              "the friend derived type can copy-assign its private base");

#ifndef __RCC__
static_assert(sizeof(std::atomic<AtomicFriendSubobjectOwner>) > 0,
              "std::atomic accepts an Owner that is a friend");
static_assert(sizeof(std::atomic<AtomicPrivateCopyBaseOwner>) > 0,
              "std::atomic accepts a derived type friended by its base");
#endif

#undef ASSERT_ATOMIC_CONSTRUCTOR_ACCESS_TRAITS
#undef ASSERT_ATOMIC_ASSIGNMENT_ACCESS_TRAITS
#undef ASSERT_ATOMIC_MOVE_CONSTRUCTOR_ACCESS_TRAITS
#undef ASSERT_ATOMIC_MOVE_ASSIGNMENT_ACCESS_TRAITS
#undef ATOMIC_IS_TRIVIALLY_COPYABLE
#undef ATOMIC_IS_COPY_CONSTRUCTIBLE
#undef ATOMIC_IS_MOVE_CONSTRUCTIBLE
#undef ATOMIC_IS_COPY_ASSIGNABLE
#undef ATOMIC_IS_MOVE_ASSIGNABLE
