struct DataPrefix {
    long long marker;
};

struct ReferenceBase {
    virtual int& value() = 0;
};

struct SideBase {
    virtual int side_value() = 0;
};

struct ReferenceDerived : DataPrefix, ReferenceBase, SideBase {
    int payload;

    int& value() override { return payload; }
    int side_value() override { return payload + 1; }
};

int main() {
    ReferenceDerived object{};
    object.marker = 41;
    object.payload = 43;

    ReferenceBase* reference_base = &object;
    SideBase* side_base = &object;
    int& result = reference_base->value();
    if (&result != &object.payload || result != 43) return 1;
    result = 47;
    if (object.payload != 47 || object.marker != 41) return 2;
    if (side_base->side_value() != 48) return 3;
    return 0;
}
