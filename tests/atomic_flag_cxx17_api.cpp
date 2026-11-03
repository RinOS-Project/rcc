#include <atomic>

int atomic_flag_cxx17_api_probe() {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
#if defined(RCC_TEST_ATOMIC_FLAG_MEMBER)
    return flag.test();
#elif defined(RCC_TEST_ATOMIC_FLAG_FREE)
    return std::atomic_flag_test(&flag);
#else
#error "select the C++20-only atomic_flag API under test"
#endif
}
