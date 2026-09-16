class SharedVirtual {
public:
    virtual int value() { return 41; }
};

extern "C" int cxx_virtual_provider() {
    SharedVirtual object{};
    return object.value();
}
