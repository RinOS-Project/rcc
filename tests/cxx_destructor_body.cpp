extern "C" void rin_free(void* pointer);

struct Tracked {
    int* count;

    Tracked(int* value) : count(value) {}

    ~Tracked() {
        *count += 1;
    }
};

struct OrderedBase {
    int* order;

    explicit OrderedBase(int* value) noexcept : order(value) {}

    ~OrderedBase() noexcept {
        *order = *order * 10 + 1;
    }
};

struct OrderedMember {
    int* order;

    explicit OrderedMember(int* value) noexcept : order(value) {}

    ~OrderedMember() noexcept {
        *order = *order * 10 + 3;
    }
};

extern "C" void record_cleanup_order(int* order);

struct OrderedCleanupToken {
    int* order;

    ~OrderedCleanupToken() noexcept {
        if (order != 0) record_cleanup_order(order);
    }
};

struct OrderedDerived : OrderedBase {
    OrderedMember member;

    explicit OrderedDerived(int* value) noexcept
        : OrderedBase(value), member(value) {}

    ~OrderedDerived() noexcept {
        *order = *order * 10 + 2;
    }
};

struct OrderedArrayElement {
    int* order;
    int tag;

    ~OrderedArrayElement() noexcept {
        *order = *order * 10 + tag;
    }
};

struct OrderedArrayDerived : OrderedBase {
    OrderedArrayElement members[2][2];

    explicit OrderedArrayDerived(int* value) noexcept : OrderedBase(value) {
        members[0][0].order = value;
        members[0][0].tag = 1;
        members[0][1].order = value;
        members[0][1].tag = 2;
        members[1][0].order = value;
        members[1][0].tag = 3;
        members[1][1].order = value;
        members[1][1].tag = 4;
    }

    ~OrderedArrayDerived() noexcept {
        *order = *order * 10 + 9;
    }
};

struct OrderedCleanupDerived {
    int* order;
    OrderedCleanupToken token;

    ~OrderedCleanupDerived() noexcept {
        *order = *order * 10 + 9;
    }
};

extern "C" void record_cleanup_order(int* order) {
    *order = *order * 10 + 4;
}

int local_destructor() {
    int count = 0;
    {
        Tracked value(&count);
    }
    return count == 1 ? 0 : 1;
}

int heap_destructor() {
    int count = 0;
    Tracked* value = new Tracked(&count);
    delete value;
    return count == 1 ? 0 : 1;
}

int explicit_subobject_destructor() {
    int order = 0;
    OrderedDerived* value = new OrderedDerived(&order);
    value->~OrderedDerived();
    rin_free(static_cast<void*>(value));
    return order == 231 ? 0 : 1;
}

int local_subobject_destructor() {
    int order = 0;
    {
        OrderedDerived value(&order);
    }
    return order == 231 ? 0 : 1;
}

int explicit_array_subobject_destructor() {
    int order = 0;
    OrderedArrayDerived* value = new OrderedArrayDerived(&order);
    value->~OrderedArrayDerived();
    rin_free(static_cast<void*>(value));
    return order == 943211 ? 0 : 1;
}

int local_array_subobject_destructor() {
    int order = 0;
    {
        OrderedArrayDerived value(&order);
    }
    return order == 943211 ? 0 : 1;
}

int local_custom_cleanup_subobject_destructor() {
    int order = 0;
    {
        OrderedCleanupDerived value;
        value.order = &order;
        value.token.order = &order;
    }
    return order == 94 ? 0 : 1;
}

int main() {
    return local_destructor() + heap_destructor() +
        explicit_subobject_destructor() + local_subobject_destructor() +
        explicit_array_subobject_destructor() +
        local_array_subobject_destructor() +
        local_custom_cleanup_subobject_destructor();
}
