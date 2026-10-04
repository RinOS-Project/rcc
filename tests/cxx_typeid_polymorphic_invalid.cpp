class DynamicType {
public:
    virtual int value() { return 1; }
};

extern "C" int probe(DynamicType& object) {
    return &typeid(object) != 0;
}
