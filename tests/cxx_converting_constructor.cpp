class Converting {
public:
    Converting(int value) : value_(value) {}

    int value() const {
        return value_;
    }

private:
    int value_;
};

static int consume(Converting object) {
    return object.value();
}

    Converting global_value = 2;

extern "C" int cxx_converting_constructor(void) {
    Converting local_value = 4;
    Converting direct_value(5);
    Converting copied_value = direct_value;
    Converting* heap_value = new Converting(6);
    int result = global_value.value() + local_value.value() +
                 direct_value.value() + copied_value.value() +
                 heap_value->value() + consume(Converting(7));
    delete heap_value;
    return result == 29 ? 0 : 1;
}
