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

    int bias = 1;

    T read_twice() const {
        return this->read() + this->value + bias;
    }
};

int main() {
    DependentClassTemplateDerived<int> integer(21);
    DependentClassTemplateDerived<long long> wide(23);
    return integer.read_twice() == 43 && wide.read_twice() == 47 ? 0 : 1;
}
