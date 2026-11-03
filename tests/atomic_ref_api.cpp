#include <atomic>

#if defined(RCC_TEST_ATOMIC_REF_CXX17)
void atomic_ref_cxx17_probe(int& value) {
    std::atomic_ref<int> reference{value};
}
#elif defined(RCC_TEST_ATOMIC_REF_IMPLICIT_CTOR)
std::atomic_ref<int> atomic_ref_implicit_probe(int& value) {
    return value;
}
#else
#error "select the atomic_ref API rule under test"
#endif
