#include <atomic>

using AtomicFunction = void();

void invalid_void_pointer_atomic_fetch(void*& pointer) {
    std::atomic<void*> value{pointer};
    (void)value.fetch_add(1);
    (void)value.fetch_sub(1);
}

void invalid_function_pointer_atomic_fetch(AtomicFunction*& pointer) {
    std::atomic<AtomicFunction*> value{pointer};
    (void)value.fetch_add(1);
    (void)value.fetch_sub(1);
}

void invalid_void_pointer_atomic_ref_fetch(void*& pointer) {
    std::atomic_ref<void*> value{pointer};
    (void)value.fetch_add(1);
    (void)value.fetch_sub(1);
}

void invalid_function_pointer_atomic_ref_fetch(AtomicFunction*& pointer) {
    std::atomic_ref<AtomicFunction*> value{pointer};
    (void)value.fetch_add(1);
    (void)value.fetch_sub(1);
}
