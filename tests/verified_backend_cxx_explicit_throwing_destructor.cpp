struct VerifiedThrowingCleanupBase {
    ~VerifiedThrowingCleanupBase() noexcept {}
};

struct VerifiedThrowingCleanupMember {
    ~VerifiedThrowingCleanupMember() noexcept {}
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
