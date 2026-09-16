extern "C" int cxx_default_destructor_count = 0;

class DefaultTracked final {
public:
    DefaultTracked() : marker(&cxx_default_destructor_count) {}

    ~DefaultTracked() {
        ++*marker;
    }

private:
    int* marker;
};

extern "C" int cxx_default_destructor_scope() {
    {
        DefaultTracked value;
        (void)value;
    }
    return cxx_default_destructor_count == 1 ? 0 : 1;
}

extern "C" int main() {
    return cxx_default_destructor_scope();
}
