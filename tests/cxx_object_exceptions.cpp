struct ExceptionPair {
    int first;
    int second;
};

extern "C" int cxx_object_exception_roundtrip() {
    try {
        ExceptionPair value{17, 25};
        throw value;
    } catch (ExceptionPair caught) {
        return caught.first + caught.second;
    }
}

extern "C" int cxx_object_exception_rethrow() {
    try {
        try {
            ExceptionPair value{9, 13};
            throw value;
        } catch (ExceptionPair caught) {
            throw;
        }
    } catch (ExceptionPair caught) {
        return caught.first * 2 + caught.second;
    }
}

extern "C" int main() {
    return cxx_object_exception_roundtrip() == 42 &&
                   cxx_object_exception_rethrow() == 31
               ? 0
               : 1;
}
