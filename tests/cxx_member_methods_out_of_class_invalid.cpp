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
