#include <atomic>

template<class T>
struct DeferredMemberBody {
    static int static_size() { return sizeof(T); }
    int member_size() const { return sizeof(T); }
};

#if defined(TEST_INVALID_STATIC_BODY)
int invalid_static_body() {
    return DeferredMemberBody<void>::static_size();
}
#elif defined(TEST_INVALID_MEMBER_BODY)
int invalid_member_body() {
    DeferredMemberBody<void> object;
    return object.member_size();
}
#elif defined(TEST_INVALID_ATOMIC_VOID_POINTER_ARITHMETIC)
void invalid_atomic_void_pointer_arithmetic(std::atomic<void*>& pointer) {
    pointer.fetch_add(1);
}
#else
DeferredMemberBody<void> unused_void_specialization;
std::atomic<void*> atomic_void_pointer;
int (*static_member_address)() = &DeferredMemberBody<int>::static_size;

int valid_static_body() {
    return DeferredMemberBody<int>::static_size();
}

int valid_member_body() {
    DeferredMemberBody<int> object;
    return object.member_size();
}
#endif
