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

struct AtomicNoMoveSubobject {
    AtomicNoMoveSubobject() {}
    AtomicNoMoveSubobject(const AtomicNoMoveSubobject&) = default;
    AtomicNoMoveSubobject& operator=(const AtomicNoMoveSubobject&) = default;
    AtomicNoMoveSubobject(AtomicNoMoveSubobject&&) = delete;
};

struct AtomicDefaultedDeletedMoves {
    AtomicDefaultedDeletedMoves() {}
    AtomicDefaultedDeletedMoves(const AtomicDefaultedDeletedMoves&) = default;
    AtomicDefaultedDeletedMoves(AtomicDefaultedDeletedMoves&&) = default;
    AtomicDefaultedDeletedMoves& operator=(
        const AtomicDefaultedDeletedMoves&) = default;
    AtomicNoMoveSubobject value;
};

static_assert(ATOMIC_IS_COPY_ASSIGNABLE(AtomicNoMoveSubobject),
              "rvalue assignment can use the available copy assignment");
static_assert(ATOMIC_IS_TRIVIALLY_COPYABLE(AtomicDefaultedDeletedMoves),
              "defaulted deleted moves preserve trivial copyability");
static_assert(ATOMIC_IS_COPY_CONSTRUCTIBLE(AtomicDefaultedDeletedMoves),
              "the copy constructor remains available");
static_assert(ATOMIC_IS_MOVE_CONSTRUCTIBLE(AtomicDefaultedDeletedMoves),
              "a defaulted deleted move constructor is ignored by overload resolution");
static_assert(ATOMIC_IS_COPY_ASSIGNABLE(AtomicDefaultedDeletedMoves),
              "the copy assignment operator remains available");
static_assert(ATOMIC_IS_MOVE_ASSIGNABLE(AtomicDefaultedDeletedMoves),
              "rvalue assignment can use the copy assignment operator");
static_assert(sizeof(std::atomic<AtomicDefaultedDeletedMoves>) > 0,
              "std::atomic accepts the copy fallback operations");

#undef ATOMIC_IS_TRIVIALLY_COPYABLE
#undef ATOMIC_IS_COPY_CONSTRUCTIBLE
#undef ATOMIC_IS_MOVE_CONSTRUCTIBLE
#undef ATOMIC_IS_COPY_ASSIGNABLE
#undef ATOMIC_IS_MOVE_ASSIGNABLE
