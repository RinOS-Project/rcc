#include <atomic>

#if defined(RCC_TEST_INVALID_ATOMIC_REFERENCE)
void invalid_atomic_reference(unsigned& value) {
    std::atomic<unsigned&> invalid{value};
}
#elif defined(RCC_TEST_INVALID_ATOMIC_REF_VOID)
using InvalidAtomicRefVoid = std::atomic_ref<void>;
#elif defined(RCC_TEST_INVALID_ATOMIC_REF_FUNCTION)
using InvalidAtomicRefFunction = std::atomic_ref<unsigned()>;
#elif defined(RCC_TEST_INVALID_ATOMIC_REF_REFERENCE)
using InvalidAtomicRefReference = std::atomic_ref<unsigned&>;
#else
#error Select one invalid atomic specialization to test
#endif
