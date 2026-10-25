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
