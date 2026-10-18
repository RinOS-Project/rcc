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

struct VerifiedArrayDestructorElement {
    int* order;
    int tag;

    ~VerifiedArrayDestructorElement() noexcept {
        *order = *order * 10 + tag;
    }
};

struct VerifiedArrayDestructorDerived : VerifiedDestructorBase {
    VerifiedArrayDestructorElement members[2][2];

    ~VerifiedArrayDestructorDerived() noexcept {
        *order = *order * 10 + 9;
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

extern "C" void verified_cxx_explicit_array_destructor(
    VerifiedArrayDestructorDerived* object) {
    object->~VerifiedArrayDestructorDerived();
}

int main() {
    return 0;
}
