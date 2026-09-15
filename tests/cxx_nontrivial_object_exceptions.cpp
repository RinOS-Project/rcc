extern "C" int cxx_nontrivial_exception_destructors = 0;

class NonTrivialException final {
public:
    int value;

    ~NonTrivialException() {
        ++cxx_nontrivial_exception_destructors;
    }
};

extern "C" int cxx_nontrivial_object_exception() {
    try {
        NonTrivialException value{};
        value.value = 7;
        throw value;
    } catch (NonTrivialException caught) {
        return caught.value;
    }
    return -1;
}

extern "C" int cxx_nontrivial_object_exception_rethrow() {
    try {
        try {
            NonTrivialException value{};
            value.value = 11;
            throw value;
        } catch (NonTrivialException caught) {
            throw;
        }
    } catch (NonTrivialException caught) {
        return caught.value;
    }
    return -1;
}

extern "C" int main() {
    return cxx_nontrivial_object_exception() == 7 &&
                   cxx_nontrivial_exception_destructors == 3 &&
                   cxx_nontrivial_object_exception_rethrow() == 11 &&
                   cxx_nontrivial_exception_destructors == 7
               ? 0
               : 1;
}
