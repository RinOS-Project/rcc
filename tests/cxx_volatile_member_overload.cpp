struct VolatileMemberProbe {
    int payload = 10;

    int read() {
        return payload + 1;
    }

    int read() volatile {
        return payload + 2;
    }

    int read() const {
        return payload + 3;
    }

    int read() const volatile {
        return payload + 4;
    }

    int read_ref() volatile & {
        return payload + 5;
    }

    int read_ref() const volatile && {
        return payload + 6;
    }
};

struct VolatileVirtualBase {
    int base_value = 71;

    virtual int dispatch() volatile {
        return base_value;
    }
};

struct VolatileVirtualDerived : VolatileVirtualBase {
    int derived_value = 72;

    int dispatch() volatile override {
        return derived_value;
    }
};

template <typename T>
struct VolatileMemberTemplate {
    T item = sizeof(T);

    T value() volatile {
        return item;
    }
};

int main() {
    VolatileMemberProbe mutable_value;
    volatile VolatileMemberProbe volatile_value;
    const VolatileMemberProbe const_value{};
    const volatile VolatileMemberProbe const_volatile_value{};
    VolatileVirtualDerived virtual_value;
    volatile VolatileVirtualBase& virtual_base = virtual_value;
    volatile VolatileMemberTemplate<int> templated_value;
    return mutable_value.read() == 11 &&
                   volatile_value.read() == 12 &&
                   const_value.read() == 13 &&
                   const_volatile_value.read() == 14 &&
                   volatile_value.read_ref() == 15 &&
                   static_cast<const volatile VolatileMemberProbe&&>(
                       const_volatile_value).read_ref() == 16 &&
                   virtual_base.dispatch() == 72 &&
                   templated_value.value() == 4
               ? 0
               : 1;
}
