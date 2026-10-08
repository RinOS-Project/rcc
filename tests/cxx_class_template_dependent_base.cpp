template <typename T>
struct DependentClassTemplateBase {
    T value;

    DependentClassTemplateBase(T initial) : value(initial) {}

    T read() const {
        return value;
    }
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
    DependentClassTemplateDerived<int> integer(21);
    DependentClassTemplateDerived<long long> wide(23);
    return integer.read_twice() == 41 && wide.read_twice() == 45 &&
                   integer.read_unsigned_biases() == 0x100000002ULL &&
                   wide.read_unsigned_biases() == 0x100000002ULL
               ? 0
               : 1;
}
