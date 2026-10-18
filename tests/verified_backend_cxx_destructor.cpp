struct VerifiedDestructorBase {
    int* order;

    ~VerifiedDestructorBase() noexcept {
        *order = *order * 10 + 1;
    }
};

struct VerifiedDestructorMember {
    int* order;

    ~VerifiedDestructorMember() noexcept {
        *order = *order * 10 + 3;
    }
};

struct VerifiedDestructorDerived : VerifiedDestructorBase {
    VerifiedDestructorMember member;

    ~VerifiedDestructorDerived() noexcept {
        *order = *order * 10 + 2;
    }
};

extern "C" void verified_cxx_explicit_destructor(
    VerifiedDestructorDerived* object) {
    object->~VerifiedDestructorDerived();
}

extern "C" int verified_cxx_scope_destructor() {
    int order = 0;
    {
        VerifiedDestructorDerived object{{&order}, {&order}};
    }
    return order;
}

int main() {
    return 0;
}
