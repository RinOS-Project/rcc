template <typename T>
struct DependentClassTemplateBase {
    T value;

    DependentClassTemplateBase(T initial) : value(initial) {}

    T read() const {
        return value;
    }
};

static int dependent_base_pack_order;

struct DependentBasePackOne {
    DependentBasePackOne() {
        dependent_base_pack_order = dependent_base_pack_order * 10 + 1;
    }
};

struct DependentBasePackTwo {
    DependentBasePackTwo() {
        dependent_base_pack_order = dependent_base_pack_order * 10 + 2;
    }
};

template <typename... Bases>
struct DependentClassTemplateBasePack : Bases... {
    DependentClassTemplateBasePack() : Bases()... {}
};

template <typename T>
struct DependentClassTemplateDerived
    : DependentClassTemplateBase<T> {
    using DependentClassTemplateBase<T>::DependentClassTemplateBase;
    using DependentClassTemplateBase<T>::value;

    T bias = -1;
    unsigned long long narrow_unsigned_bias = 1u;
    unsigned long long wide_unsigned_bias = 0x100000001ULL;

    T read_twice() const {
        return this->read() + this->value + bias;
    }

    unsigned long long read_unsigned_biases() const {
        return narrow_unsigned_bias + wide_unsigned_bias;
    }
};

template <typename T>
struct DependentVirtualClassTemplateDerived
    : virtual DependentClassTemplateBase<T> {
    DependentVirtualClassTemplateDerived(T initial)
        : DependentClassTemplateBase<T>(initial) {}

    T read_virtual_base() const {
        return this->value;
    }
};

template <typename T>
struct DependentVirtualDispatchBase {
    explicit DependentVirtualDispatchBase(T initial) : value(initial) {}

    virtual T read() const {
        return value;
    }

    T value;
};

template <typename T>
struct DependentVirtualDispatchDerived
    : virtual DependentVirtualDispatchBase<T> {
    explicit DependentVirtualDispatchDerived(T initial)
        : DependentVirtualDispatchBase<T>(initial) {}

    T read() const override {
        return this->value + 1;
    }
};

struct DependentAccessPrefix {
    int padding;
};

template <typename T>
struct DependentUsingBaseOne {
    T value;

    int choose(T) const {
        return 1;
    }
};

template <typename T>
struct DependentUsingBaseTwo {
    T value;

    int choose(long) const {
        return 2;
    }
};

template <typename T>
struct DependentUsingDerived : DependentUsingBaseOne<T>,
                               DependentUsingBaseTwo<T> {
    using DependentUsingBaseTwo<T>::value;
    using DependentUsingBaseTwo<T>::choose;
};

template <typename T>
struct DependentDmiMember {
    T value;

    DependentDmiMember(T initial) : value(initial) {}
};

template <typename T>
struct DependentDmiContainer {
    DependentDmiMember<T> nested{7};
    T scaled = 3 * 4;
};

template <typename T>
struct DependentDmiBase {};

template <typename T>
struct DependentDmiReference : DependentDmiBase<T> {
    T scaled = 3 * 4;
    T adjusted = this->scaled + 2;
    T unqualified_adjusted = scaled + 3;
    DependentDmiReference() {}
};

template <typename T>
struct DependentAccessBase {
    T public_value;

    T public_read() const {
        return public_value;
    }

protected:
    T protected_value;

    T protected_read() const {
        return protected_value;
    }
};

template <typename T>
struct DependentPrivateAccessDerived
    : DependentAccessPrefix, private DependentAccessBase<T> {
    void write(T initial) {
        this->public_value = initial;
        this->protected_value = initial + 1;
    }

    T read() const {
        return this->public_value + this->protected_value +
               this->public_read() + this->protected_read();
    }
};

template <typename T>
struct DependentProtectedAccessDerived
    : DependentAccessPrefix, protected DependentAccessBase<T> {
    void write(T initial) {
        this->public_value = initial;
        this->protected_value = initial + 1;
    }

    T read() const {
        return this->public_value + this->protected_value +
               this->public_read() + this->protected_read();
    }
};

template <typename T>
struct DependentHidingDerived : DependentAccessBase<T> {
    T public_value;
    T public_extra;

    void write(T initial) {
        this->public_value = initial;
        this->public_extra = initial + 2;
    }

    T public_read() const {
        return this->public_value + this->public_extra;
    }

    T read() const {
        return this->public_value + this->public_extra +
               this->public_read();
    }
};

DependentPrivateAccessDerived<int> dependent_private_access;
DependentProtectedAccessDerived<int> dependent_protected_access;
DependentHidingDerived<int> dependent_hiding;

int main() {
    dependent_base_pack_order = 0;
    DependentClassTemplateBasePack<> empty_pack;
    if (dependent_base_pack_order != 0) return 2;

    dependent_base_pack_order = 0;
    DependentClassTemplateBasePack<DependentBasePackOne> one_base;
    if (dependent_base_pack_order != 1) return 3;

    dependent_base_pack_order = 0;
    DependentClassTemplateBasePack<DependentBasePackOne,
                                   DependentBasePackTwo> two_bases;
    if (dependent_base_pack_order != 12) return 4;

    DependentClassTemplateDerived<int> integer(21);
    DependentClassTemplateDerived<long long> wide(23);
    DependentVirtualClassTemplateDerived<int> virtual_integer(29);
    DependentVirtualClassTemplateDerived<long long> virtual_wide(31);
    DependentClassTemplateBase<int>* virtual_integer_base = &virtual_integer;
    DependentClassTemplateBase<long long>* virtual_wide_base = &virtual_wide;
    DependentVirtualDispatchDerived<int> virtual_dispatch_integer{37};
    DependentVirtualDispatchDerived<long long> virtual_dispatch_wide{41};
    DependentVirtualDispatchBase<int>* virtual_dispatch_integer_base =
        &virtual_dispatch_integer;
    DependentVirtualDispatchBase<long long>* virtual_dispatch_wide_base =
        &virtual_dispatch_wide;
    DependentDmiContainer<int> dependent_dmi_integer{};
    DependentDmiContainer<long long> dependent_dmi_wide{};
    DependentDmiReference<int> dependent_dmi_reference_integer{};
    DependentDmiReference<long long> dependent_dmi_reference_wide{};
    DependentUsingDerived<int> dependent_using;
    DependentUsingBaseOne<int>* dependent_using_base_one = &dependent_using;
    DependentUsingBaseTwo<int>* dependent_using_base_two = &dependent_using;
    dependent_private_access.write(10);
    dependent_protected_access.write(20);
    dependent_hiding.write(30);
    dependent_using_base_one->value = 10;
    dependent_using_base_two->value = 20;
    return integer.read_twice() == 41 && wide.read_twice() == 45 &&
                   integer.read_unsigned_biases() == 0x100000002ULL &&
                   wide.read_unsigned_biases() == 0x100000002ULL &&
                   virtual_integer.read_virtual_base() == 29 &&
                   virtual_wide.read_virtual_base() == 31 &&
                   virtual_integer_base->read() == 29 &&
                   virtual_wide_base->read() == 31 &&
                   virtual_dispatch_integer_base->read() == 38 &&
                   virtual_dispatch_wide_base->read() == 42 &&
                   dependent_private_access.read() == 42 &&
                   dependent_protected_access.read() == 82 &&
                   dependent_hiding.read() == 124 &&
                   dependent_using.value == 20 &&
                   dependent_using.choose(1) == 2 &&
                   dependent_dmi_integer.nested.value == 7 &&
                   dependent_dmi_integer.scaled == 12 &&
                   dependent_dmi_wide.nested.value == 7 &&
                   dependent_dmi_wide.scaled == 12 &&
                   dependent_dmi_reference_integer.adjusted == 14 &&
                   dependent_dmi_reference_wide.adjusted == 14 &&
                   dependent_dmi_reference_integer.unqualified_adjusted == 15 &&
                   dependent_dmi_reference_wide.unqualified_adjusted == 15
               ? 0
               : 9;
}
