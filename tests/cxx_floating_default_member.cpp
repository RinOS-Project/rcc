class FloatingDefaults {
public:
    float fraction = 1.5f;
    int whole = 2;

    int total() const {
        return (int)(fraction * 2.0f) + whole;
    }
};

class FloatingConstructorDefaults {
public:
    explicit FloatingConstructorDefaults(int value) : marker_(value) {}

    int total() const {
        return (int)(fraction * 2.0f) + marker_;
    }

private:
    float fraction = 1.25f;
    int marker_;
};

extern "C" int cxx_floating_default_member(void) {
    FloatingDefaults local{};
    FloatingDefaults* heap = new FloatingDefaults{};
    FloatingDefaults* array = new FloatingDefaults[2]{};
    FloatingConstructorDefaults constructed(4);
    FloatingConstructorDefaults* constructed_heap =
        new FloatingConstructorDefaults(5);
    int result = local.total() + heap->total() +
                 array[0].total() + array[1].total();
    result += constructed.total() + constructed_heap->total();
    delete heap;
    delete[] array;
    delete constructed_heap;
    return result == 33 ? 0 : 1;
}
