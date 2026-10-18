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

struct OrderedDerived : OrderedBase {
    OrderedMember member;

    explicit OrderedDerived(int* value) noexcept
        : OrderedBase(value), member(value) {}

    ~OrderedDerived() noexcept {
        *order = *order * 10 + 2;
    }
};

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

int main() {
    return local_destructor() + heap_destructor() +
        explicit_subobject_destructor() + local_subobject_destructor();
}
