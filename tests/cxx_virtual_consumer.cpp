class SharedVirtual {
public:
    virtual int value() { return 41; }
};

extern "C" int cxx_virtual_provider();

extern "C" int main() {
    SharedVirtual object{};
    return cxx_virtual_provider() == 41 && object.value() == 41 ? 0 : 1;
}
