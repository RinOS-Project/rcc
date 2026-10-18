struct VerifiedThrowingCleanupBase {
    ~VerifiedThrowingCleanupBase() noexcept(false) {}
};

struct VerifiedThrowingCleanupMember {
    ~VerifiedThrowingCleanupMember() noexcept(false) {}
};

struct VerifiedThrowingCleanupDerived : VerifiedThrowingCleanupBase {
    VerifiedThrowingCleanupMember member;
    ~VerifiedThrowingCleanupDerived() noexcept(false) {}
};

extern "C" void verified_cxx_explicit_throwing_destructor(
    VerifiedThrowingCleanupDerived* object) {
    object->~VerifiedThrowingCleanupDerived();
}

int main() {
    return 0;
}
