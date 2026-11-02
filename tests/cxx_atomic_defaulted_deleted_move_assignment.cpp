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

struct AtomicNoMoveAssignmentSubobject {
    AtomicNoMoveAssignmentSubobject() {}
    AtomicNoMoveAssignmentSubobject(
        const AtomicNoMoveAssignmentSubobject&) = default;
    AtomicNoMoveAssignmentSubobject& operator=(
        const AtomicNoMoveAssignmentSubobject&) = default;
    AtomicNoMoveAssignmentSubobject& operator=(
        AtomicNoMoveAssignmentSubobject&&) = delete;
};

static_assert(
    ATOMIC_IS_COPY_ASSIGNABLE(AtomicNoMoveAssignmentSubobject),
    "the copy assignment operator remains available");
static_assert(
    !ATOMIC_IS_MOVE_ASSIGNABLE(AtomicNoMoveAssignmentSubobject),
    "an explicitly deleted move assignment remains an overload candidate");

struct AtomicDefaultedDeletedMoveAssignment {
    AtomicDefaultedDeletedMoveAssignment() {}
    AtomicDefaultedDeletedMoveAssignment(
        const AtomicDefaultedDeletedMoveAssignment&) = default;
    AtomicDefaultedDeletedMoveAssignment& operator=(
        const AtomicDefaultedDeletedMoveAssignment&) = default;
    AtomicDefaultedDeletedMoveAssignment& operator=(
        AtomicDefaultedDeletedMoveAssignment&&) = default;
    AtomicNoMoveAssignmentSubobject value;
};

static_assert(
    ATOMIC_IS_TRIVIALLY_COPYABLE(AtomicDefaultedDeletedMoveAssignment),
    "a defaulted deleted move assignment preserves trivial copyability");
static_assert(
    ATOMIC_IS_COPY_CONSTRUCTIBLE(AtomicDefaultedDeletedMoveAssignment),
    "the copy constructor remains available");
static_assert(
    ATOMIC_IS_MOVE_CONSTRUCTIBLE(AtomicDefaultedDeletedMoveAssignment),
    "rvalue construction can use the copy constructor");
static_assert(
    ATOMIC_IS_COPY_ASSIGNABLE(AtomicDefaultedDeletedMoveAssignment),
    "the copy assignment operator remains available");
static_assert(
    ATOMIC_IS_MOVE_ASSIGNABLE(AtomicDefaultedDeletedMoveAssignment),
    "a defaulted deleted move assignment is ignored by overload resolution");
static_assert(
    sizeof(std::atomic<AtomicDefaultedDeletedMoveAssignment>) > 0,
    "std::atomic accepts the copy assignment fallback");

void atomic_defaulted_deleted_move_assignment_falls_back(
    AtomicDefaultedDeletedMoveAssignment& destination,
    AtomicDefaultedDeletedMoveAssignment&& source) {
    destination = static_cast<AtomicDefaultedDeletedMoveAssignment&&>(source);
}

#undef ATOMIC_IS_TRIVIALLY_COPYABLE
#undef ATOMIC_IS_COPY_CONSTRUCTIBLE
#undef ATOMIC_IS_MOVE_CONSTRUCTIBLE
#undef ATOMIC_IS_COPY_ASSIGNABLE
#undef ATOMIC_IS_MOVE_ASSIGNABLE
