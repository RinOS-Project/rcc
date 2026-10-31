#include <atomic>

struct AtomicWaitAggregate {
    unsigned char tag;
    unsigned value[3];
};

static_assert(sizeof(AtomicWaitAggregate) == 16u,
              "aggregate wait API fixture must exceed lock-free widths");

template<class T>
struct AtomicWaitPointerProbe {
    T value;
};

template<class T>
void atomic_wait_pointer_probe(AtomicWaitPointerProbe<T>* object,
                               T value) {
    object->value = value;
}

void atomic_wait_template_address_deduction(
    AtomicWaitPointerProbe<unsigned>& value) {
    atomic_wait_pointer_probe(&value, 1u);
}

void atomic_wait_api_surface(
    std::atomic<unsigned>& scalar,
#if !defined(RCC_STDLIB_REFERENCE)
    volatile std::atomic<unsigned>& volatile_scalar,
#endif
    std::atomic<AtomicWaitAggregate>& aggregate,
    AtomicWaitAggregate& referenced_aggregate,
    std::atomic_flag& flag,
    std::atomic<unsigned*>& pointer_atomic,
    unsigned& referenced_scalar,
    unsigned*& referenced_pointer) {
    scalar.wait(0u, std::memory_order_acquire);
    scalar.notify_one();
    scalar.notify_all();
    std::atomic_wait(&scalar, 0u);
    std::atomic_wait_explicit(&scalar, 0u, std::memory_order_relaxed);
    std::atomic_notify_one(&scalar);
    std::atomic_notify_all(&scalar);
    std::atomic_wait(&pointer_atomic, static_cast<unsigned*>(nullptr));
    std::atomic_notify_all(&pointer_atomic);

#if !defined(RCC_STDLIB_REFERENCE)
    volatile_scalar.wait(0u, std::memory_order_relaxed);
    volatile_scalar.notify_one();
    std::atomic_wait(&volatile_scalar, 0u);
    std::atomic_notify_one(&volatile_scalar);
#endif

    aggregate.wait(AtomicWaitAggregate{0u, {0u, 0u, 0u}});
    aggregate.notify_all();
    std::atomic_wait(&aggregate, AtomicWaitAggregate{0u, {0u, 0u, 0u}});
    std::atomic_notify_one(&aggregate);

    std::atomic_ref<AtomicWaitAggregate> aggregate_ref(referenced_aggregate);
    aggregate_ref.wait(AtomicWaitAggregate{0u, {0u, 0u, 0u}});
    aggregate_ref.notify_all();

    flag.wait(false, std::memory_order_acquire);
    flag.notify_one();
    flag.notify_all();
    std::atomic_flag_wait(&flag, false);
    std::atomic_flag_wait_explicit(&flag, false,
                                   std::memory_order_relaxed);
    std::atomic_flag_notify_one(&flag);
    std::atomic_flag_notify_all(&flag);

    std::atomic_ref<unsigned> scalar_ref(referenced_scalar);
    scalar_ref.wait(0u);
    scalar_ref.notify_all();

    std::atomic_ref<unsigned*> pointer_ref(referenced_pointer);
    pointer_ref.wait(nullptr, std::memory_order_acquire);
    pointer_ref.notify_one();
}
