#include <atomic>

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

struct AtomicInheritedSpecialMemberBase {
    AtomicInheritedSpecialMemberBase() = default;
    AtomicInheritedSpecialMemberBase(
        const AtomicInheritedSpecialMemberBase&) = default;
    AtomicInheritedSpecialMemberBase(
        AtomicInheritedSpecialMemberBase&&) = default;
    AtomicInheritedSpecialMemberBase& operator=(
        const AtomicInheritedSpecialMemberBase&) = default;
    AtomicInheritedSpecialMemberBase& operator=(
        AtomicInheritedSpecialMemberBase&&) = default;
};

struct AtomicInheritedSpecialMemberDerived
    : AtomicInheritedSpecialMemberBase {};

static_assert(ATOMIC_IS_TRIVIALLY_COPYABLE(
    AtomicInheritedSpecialMemberDerived));
static_assert(ATOMIC_IS_COPY_CONSTRUCTIBLE(
    AtomicInheritedSpecialMemberDerived));
static_assert(ATOMIC_IS_MOVE_CONSTRUCTIBLE(
    AtomicInheritedSpecialMemberDerived));
static_assert(ATOMIC_IS_COPY_ASSIGNABLE(
    AtomicInheritedSpecialMemberDerived));
static_assert(ATOMIC_IS_MOVE_ASSIGNABLE(
    AtomicInheritedSpecialMemberDerived));
#if !defined(__RCC__)
static_assert(sizeof(std::atomic<AtomicInheritedSpecialMemberDerived>) > 0);
#endif

struct AtomicNoMoveSpecialMemberSubobject {
    AtomicNoMoveSpecialMemberSubobject() = default;
    AtomicNoMoveSpecialMemberSubobject(
        const AtomicNoMoveSpecialMemberSubobject&) = default;
    AtomicNoMoveSpecialMemberSubobject(
        AtomicNoMoveSpecialMemberSubobject&&) = delete;
    AtomicNoMoveSpecialMemberSubobject& operator=(
        const AtomicNoMoveSpecialMemberSubobject&) = default;
    AtomicNoMoveSpecialMemberSubobject& operator=(
        AtomicNoMoveSpecialMemberSubobject&&) = delete;
};

struct AtomicDefaultedDeletedMoveSpecialMemberBase {
    AtomicDefaultedDeletedMoveSpecialMemberBase() = default;
    AtomicDefaultedDeletedMoveSpecialMemberBase(
        const AtomicDefaultedDeletedMoveSpecialMemberBase&) = default;
    AtomicDefaultedDeletedMoveSpecialMemberBase(
        AtomicDefaultedDeletedMoveSpecialMemberBase&&) = default;
    AtomicDefaultedDeletedMoveSpecialMemberBase& operator=(
        const AtomicDefaultedDeletedMoveSpecialMemberBase&) = default;
    AtomicDefaultedDeletedMoveSpecialMemberBase& operator=(
        AtomicDefaultedDeletedMoveSpecialMemberBase&&) = default;
    AtomicNoMoveSpecialMemberSubobject value;
};

struct AtomicDefaultedDeletedMoveSpecialMemberDerived
    : AtomicDefaultedDeletedMoveSpecialMemberBase {};

#if defined(__RCC__)
static_assert(__is_assignable(AtomicDefaultedDeletedMoveSpecialMemberBase&,
                              const AtomicDefaultedDeletedMoveSpecialMemberDerived&));
static_assert(__is_assignable(AtomicDefaultedDeletedMoveSpecialMemberBase&,
                              AtomicDefaultedDeletedMoveSpecialMemberDerived&&));
#else
static_assert(std::is_assignable_v<AtomicDefaultedDeletedMoveSpecialMemberBase&,
                                   const AtomicDefaultedDeletedMoveSpecialMemberDerived&>);
static_assert(std::is_assignable_v<AtomicDefaultedDeletedMoveSpecialMemberBase&,
                                   AtomicDefaultedDeletedMoveSpecialMemberDerived&&>);
#endif

static_assert(ATOMIC_IS_TRIVIALLY_COPYABLE(
    AtomicDefaultedDeletedMoveSpecialMemberBase));
static_assert(ATOMIC_IS_COPY_CONSTRUCTIBLE(
    AtomicDefaultedDeletedMoveSpecialMemberBase));
static_assert(ATOMIC_IS_MOVE_CONSTRUCTIBLE(
    AtomicDefaultedDeletedMoveSpecialMemberBase));
static_assert(ATOMIC_IS_COPY_ASSIGNABLE(
    AtomicDefaultedDeletedMoveSpecialMemberBase));
static_assert(ATOMIC_IS_MOVE_ASSIGNABLE(
    AtomicDefaultedDeletedMoveSpecialMemberBase));

static_assert(ATOMIC_IS_TRIVIALLY_COPYABLE(
    AtomicDefaultedDeletedMoveSpecialMemberDerived));
static_assert(ATOMIC_IS_COPY_CONSTRUCTIBLE(
    AtomicDefaultedDeletedMoveSpecialMemberDerived));
static_assert(ATOMIC_IS_MOVE_CONSTRUCTIBLE(
    AtomicDefaultedDeletedMoveSpecialMemberDerived));
static_assert(ATOMIC_IS_COPY_ASSIGNABLE(
    AtomicDefaultedDeletedMoveSpecialMemberDerived));
static_assert(ATOMIC_IS_MOVE_ASSIGNABLE(
    AtomicDefaultedDeletedMoveSpecialMemberDerived));
void assign_inherited_defaulted_copy(
    AtomicDefaultedDeletedMoveSpecialMemberDerived& destination,
    const AtomicDefaultedDeletedMoveSpecialMemberDerived& source) {
    destination = source;
}
void assign_inherited_defaulted_move(
    AtomicDefaultedDeletedMoveSpecialMemberDerived& destination,
    AtomicDefaultedDeletedMoveSpecialMemberDerived&& source) {
    destination = static_cast<AtomicDefaultedDeletedMoveSpecialMemberDerived&&>(
        source);
}

struct AtomicInheritedCopyAssignmentResult {};
struct AtomicInheritedMoveAssignmentResult {};

struct AtomicInheritedAssignmentBase {
    AtomicInheritedCopyAssignmentResult operator=(
        const AtomicInheritedAssignmentBase&);
    AtomicInheritedMoveAssignmentResult operator=(
        AtomicInheritedAssignmentBase&&);
};

struct AtomicInheritedAssignmentDerived : AtomicInheritedAssignmentBase {
    using AtomicInheritedAssignmentBase::operator=;
};

AtomicInheritedMoveAssignmentResult inherited_assignment_prefers_rvalue(
    AtomicInheritedAssignmentDerived& destination,
    AtomicInheritedAssignmentBase&& source) {
    return destination =
        static_cast<AtomicInheritedAssignmentBase&&>(source);
}

#if !defined(__RCC__)
static_assert(std::is_same_v<
    decltype(std::declval<AtomicInheritedAssignmentDerived&>() =
             std::declval<AtomicInheritedAssignmentBase&&>()),
    AtomicInheritedMoveAssignmentResult>);
#endif
#if !defined(__RCC__)
static_assert(sizeof(std::atomic<AtomicDefaultedDeletedMoveSpecialMemberBase>) > 0);
static_assert(sizeof(std::atomic<AtomicDefaultedDeletedMoveSpecialMemberDerived>) > 0);
#endif

#undef ATOMIC_IS_TRIVIALLY_COPYABLE
#undef ATOMIC_IS_COPY_CONSTRUCTIBLE
#undef ATOMIC_IS_MOVE_CONSTRUCTIBLE
#undef ATOMIC_IS_COPY_ASSIGNABLE
#undef ATOMIC_IS_MOVE_ASSIGNABLE
