extern "C" void* rin_malloc(unsigned long size);
extern "C" void rin_free(void* pointer);

class VerifiedThrowingCalleeMember {
public:
    explicit VerifiedThrowingCalleeMember(int* value) noexcept
        : order(value) {}

    ~VerifiedThrowingCalleeMember() noexcept {
        *order = *order * 10 + 1;
    }

    int* order;
};

class VerifiedThrowingCalleeLeft {
public:
    explicit VerifiedThrowingCalleeLeft(int* value) noexcept
        : order(value), member(value) {}

    ~VerifiedThrowingCalleeLeft() noexcept {
        *order = *order * 10 + 2;
    }

    int* order;
    VerifiedThrowingCalleeMember member;
};

class VerifiedThrowingCalleeRight {
public:
    explicit VerifiedThrowingCalleeRight(int* value) noexcept
        : order(value) {}

    ~VerifiedThrowingCalleeRight() noexcept {
        *order = *order * 10 + 3;
    }

    int* order;
};

class VerifiedThrowingCalleeDerived : public VerifiedThrowingCalleeLeft,
                                     public VerifiedThrowingCalleeRight {
public:
    VerifiedThrowingCalleeDerived(
        int* value, VerifiedThrowingCalleeDerived** receiver_slot,
        int* throw_flag) noexcept
        : VerifiedThrowingCalleeLeft(value),
          VerifiedThrowingCalleeRight(value), order(value),
          receiver(receiver_slot), should_throw(throw_flag) {}

    ~VerifiedThrowingCalleeDerived() noexcept(false) {
        *receiver = 0;
        *order = *order * 10 + 9;
        if (*should_throw) throw 47;
    }

    int* order;
    VerifiedThrowingCalleeDerived** receiver;
    int* should_throw;
};

extern "C" int verified_cxx_explicit_throwing_destructor_callee(
    int* order, VerifiedThrowingCalleeDerived** receiver,
    VerifiedThrowingCalleeDerived** storage, int* should_throw) {
    VerifiedThrowingCalleeDerived* value =
        new VerifiedThrowingCalleeDerived(order, receiver, should_throw);
    *storage = value;
    value->~VerifiedThrowingCalleeDerived();
    return -1;
}

extern "C" int verified_cxx_explicit_throwing_destructor_case(
    int should_throw_value) {
    int order = 0;
    int should_throw = should_throw_value;
    int caught = 0;
    VerifiedThrowingCalleeDerived* receiver = 0;
    VerifiedThrowingCalleeDerived* storage = 0;
    try {
        verified_cxx_explicit_throwing_destructor_callee(
            &order, &receiver, &storage, &should_throw);
    } catch (int exception_value) {
        caught = exception_value;
    }
    rin_free(static_cast<void*>(storage));
    return caught == (should_throw_value ? 47 : 0) &&
            order == 9321 && receiver == 0
        ? 0 : 1;
}

int main() {
    return 0;
}
