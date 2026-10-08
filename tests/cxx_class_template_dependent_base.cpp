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
    return integer.read_twice() == 41 && wide.read_twice() == 45 &&
                   integer.read_unsigned_biases() == 0x100000002ULL &&
                   wide.read_unsigned_biases() == 0x100000002ULL
               ? 0
               : 5;
}
