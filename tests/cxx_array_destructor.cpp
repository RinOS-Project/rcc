extern "C" int cxx_array_destruct_count;

struct ArrayDestructor {
    ~ArrayDestructor() {
        cxx_array_destruct_count += 1;
    }
};

struct ConstructedArrayDestructor {
    int value;

    ConstructedArrayDestructor(int input) : value(input) {}

    ~ConstructedArrayDestructor() {
        cxx_array_destruct_count = cxx_array_destruct_count * 10 + value;
    }
};

extern "C" int cxx_array_destructor(void) {
    cxx_array_destruct_count = 0;
    ArrayDestructor* values = new ArrayDestructor[3];
    delete[] values;
    return cxx_array_destruct_count == 3 ? 0 : 1;
}

extern "C" int cxx_array_destructor_value_init(void) {
    cxx_array_destruct_count = 0;
    ArrayDestructor* values = new ArrayDestructor[3]();
    delete[] values;
    return cxx_array_destruct_count == 3 ? 0 : 1;
}

extern "C" int cxx_constructed_array_destructor(void) {
    cxx_array_destruct_count = 0;
    ConstructedArrayDestructor* values =
        new ConstructedArrayDestructor[2]{2, 3};
    delete[] values;
    return cxx_array_destruct_count == 32 ? 0 : 1;
}

int main() {
    return cxx_array_destructor() + cxx_array_destructor_value_init() +
           cxx_constructed_array_destructor();
}
