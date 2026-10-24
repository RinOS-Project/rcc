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

struct VerifiedImplicitArrayDefaultOwner {
    int* order;
    VerifiedArrayDestructorElement members[2][2];

    explicit VerifiedImplicitArrayDefaultOwner(int* target) noexcept
        : order(target) {
        members[0][0].order = target;
        members[0][0].tag = 1;
        members[0][1].order = target;
        members[0][1].tag = 2;
        members[1][0].order = target;
        members[1][0].tag = 3;
        members[1][1].order = target;
        members[1][1].tag = 4;
    }
};

extern "C" void verified_record_cleanup_order(int* order) {
    *order = *order * 10 + 4;
}

struct VerifiedCleanupToken {
    int* order;

    ~VerifiedCleanupToken() noexcept {
        if (order != 0) verified_record_cleanup_order(order);
    }
};

struct VerifiedCleanupDerived {
    int* order;
    VerifiedCleanupToken token;

    ~VerifiedCleanupDerived() noexcept {
        *order = *order * 10 + 9;
    }
};

struct VerifiedVirtualDestructorBase {
    int* order;

    ~VerifiedVirtualDestructorBase() noexcept {
        *order = *order * 10 + 1;
    }
};

struct VerifiedVirtualDestructorDerived
    : virtual VerifiedVirtualDestructorBase {
    int* order;

    ~VerifiedVirtualDestructorDerived() noexcept {
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

extern "C" void verified_cxx_explicit_cleanup_destructor(
    VerifiedCleanupDerived* object) {
    object->~VerifiedCleanupDerived();
}

extern "C" void verified_cxx_explicit_virtual_destructor(
    VerifiedVirtualDestructorDerived* object) {
    object->~VerifiedVirtualDestructorDerived();
}

extern "C" int verified_cxx_implicit_array_default_cleanup() {
    int order = 0;
    {
        VerifiedImplicitArrayDefaultOwner object(&order);
    }
    return order;
}

int main() {
    return 0;
}
