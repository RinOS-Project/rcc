struct OutOfClassOperatorProbe {
    int operator+(int value) const;
};

long OutOfClassOperatorProbe::operator+(int value) const { return value; }

struct OutOfClassOperatorRedefinitionProbe {
    int operator+(int value) const;
};

int OutOfClassOperatorRedefinitionProbe::operator+(int value) const {
    return value;
}

struct OutOfClassConversionProbe {
    operator bool() const noexcept;
};

OutOfClassConversionProbe::operator int() const noexcept { return 0; }

struct OutOfClassPointerConversionProbe {
    operator int*() const;
};

OutOfClassPointerConversionProbe::operator const int*() const {
    return nullptr;
}

struct OutOfClassConversionNoexceptProbe {
    operator bool() const noexcept;
};

OutOfClassConversionNoexceptProbe::operator bool() const { return false; }

struct OutOfClassConversionRedefinitionProbe {
    operator bool() const noexcept;
};

OutOfClassConversionRedefinitionProbe::operator bool() const noexcept {
    return false;
}

OutOfClassConversionRedefinitionProbe::operator bool() const noexcept {
    return true;
}

struct OutOfClassMemberTemplateDefinitionProbe {
    template<typename T>
    T value(T input);
};

template<typename T>
long OutOfClassMemberTemplateDefinitionProbe::value(T input) {
    return static_cast<long>(input);
}

struct OutOfClassConstexprTemplateSpecifierProbe {
    template<typename T>
    constexpr int value(T input);
};

template<typename T>
int OutOfClassConstexprTemplateSpecifierProbe::value(T input) {
    return static_cast<int>(input);
}

struct OutOfClassConstevalTemplateSpecifierProbe {
    template<typename T>
    consteval int value(T input);
};

template<typename T>
int OutOfClassConstevalTemplateSpecifierProbe::value(T input) {
    return static_cast<int>(input);
}

struct OutOfClassConstexprConstructorSpecifierProbe {
    constexpr OutOfClassConstexprConstructorSpecifierProbe();
};

OutOfClassConstexprConstructorSpecifierProbe::
    OutOfClassConstexprConstructorSpecifierProbe() {}

struct OutOfClassConstexprDestructorSpecifierProbe {
    constexpr ~OutOfClassConstexprDestructorSpecifierProbe();
};

OutOfClassConstexprDestructorSpecifierProbe::
    ~OutOfClassConstexprDestructorSpecifierProbe() {}

struct OutOfClassConstexprConversionSpecifierProbe {
    constexpr operator bool() const;
};

OutOfClassConstexprConversionSpecifierProbe::operator bool() const {
    return true;
}

struct OutOfClassConstexprMethodSpecifierProbe {
    constexpr int value() const;
};

int OutOfClassConstexprMethodSpecifierProbe::value() const {
    return 1;
}

template<typename T>
struct OutOfClassTemplateOperatorProbe {
    T operator+(T value) const;
};

template<typename T>
long OutOfClassTemplateOperatorProbe<T>::operator+(T value) const {
    return static_cast<long>(value);
}

template<typename T>
struct OutOfClassTemplateSubscriptProbe {
    T operator[](T index) const;
};

template<typename T>
long OutOfClassTemplateSubscriptProbe<T>::operator[](T index) const {
    return static_cast<long>(index);
}

template<typename T>
struct OutOfClassTemplateArrowProbe {
    T* operator->() const;
};

template<typename T>
long* OutOfClassTemplateArrowProbe<T>::operator->() const {
    return nullptr;
}

int OutOfClassOperatorRedefinitionProbe::operator+(int value) const {
    return value;
}

struct OutOfClassDefinitionProbe {
    int value();
    int replace(int value);
};

int OutOfClassDefinitionProbe::value() const {
    return 1;
}

int OutOfClassDefinitionProbe::replace(int value = 7) {
    return value;
}

int OutOfClassDefinitionProbe::value() {
    return 2;
}

int OutOfClassDefinitionProbe::value() {
    return 3;
}

struct OutOfClassConstructorProbe {
    explicit OutOfClassConstructorProbe(int);
};

OutOfClassConstructorProbe::OutOfClassConstructorProbe(long value) {}

OutOfClassConstructorProbe::OutOfClassConstructorProbe(int value = 1) {}

OutOfClassConstructorProbe::OutOfClassConstructorProbe(int value) {}

OutOfClassConstructorProbe::OutOfClassConstructorProbe(int value) {}

struct OutOfClassDestructorProbe {
    ~OutOfClassDestructorProbe() noexcept;
};

OutOfClassDestructorProbe::~OutOfClassDestructorProbe(int value) noexcept {}

OutOfClassDestructorProbe::~OutOfClassDestructorProbe() noexcept {}

OutOfClassDestructorProbe::~OutOfClassDestructorProbe() noexcept {}

OutOfClassDestructorProbe::~OutOfClassDestructorProbe(int value) noexcept {}

struct OutOfClassDestructorNoexceptProbe {
    ~OutOfClassDestructorNoexceptProbe() noexcept;
};

OutOfClassDestructorNoexceptProbe::~OutOfClassDestructorNoexceptProbe() {}
