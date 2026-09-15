struct ExceptionPair {
    int first;
    int second;
};

struct BaseException {
    int base;
};

struct DerivedException : public BaseException {
    int extra;
};

struct PrefixException {
    int prefix;
};

struct OffsetDerivedException : public PrefixException,
                               public BaseException {
    int extra;
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

extern "C" int cxx_derived_object_exception() {
    try {
        DerivedException value;
        value.base = 29;
        value.extra = 13;
        throw value;
    } catch (BaseException caught) {
        return caught.base;
    } catch (...) {
        return 0;
    }
}

extern "C" int cxx_offset_base_object_exception() {
    try {
        OffsetDerivedException value;
        value.prefix = 3;
        value.base = 47;
        value.extra = 19;
        throw value;
    } catch (BaseException caught) {
        return caught.base;
    } catch (...) {
        return 0;
    }
}

extern "C" int cxx_scalar_exception_reference() {
    try {
        int value = 19;
        throw value;
    } catch (int& caught) {
        caught += 4;
        return caught;
    }
}

extern "C" int cxx_const_scalar_exception_reference() {
    try {
        throw 23;
    } catch (const int& caught) {
        return caught + 6;
    }
}

extern "C" int cxx_derived_object_exception_reference() {
    try {
        DerivedException value;
        value.base = 31;
        value.extra = 11;
        throw value;
    } catch (BaseException& caught) {
        caught.base += 6;
        return caught.base;
    }
}

extern "C" int cxx_object_exception_rvalue_reference() {
    try {
        ExceptionPair value{14, 16};
        throw value;
    } catch (ExceptionPair&& caught) {
        return caught.first + caught.second;
    }
}

extern "C" int main() {
    return cxx_object_exception_roundtrip() == 42 &&
                   cxx_object_exception_rethrow() == 31 &&
                   cxx_derived_object_exception() == 29 &&
                   cxx_offset_base_object_exception() == 47 &&
                   cxx_scalar_exception_reference() == 23 &&
                   cxx_const_scalar_exception_reference() == 29 &&
                   cxx_derived_object_exception_reference() == 37 &&
                   cxx_object_exception_rvalue_reference() == 30
               ? 0
               : 1;
}
