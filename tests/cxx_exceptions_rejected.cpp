extern "C" int cxx_exception_leaf(int value) {
    if (value) throw value;
    return 0;
}

extern "C" int cxx_exception_path(int value) {
    try {
        cxx_exception_leaf(value);
    } catch (int caught) {
        return caught + 1;
    }
    return 0;
}

extern "C" int cxx_exception_ellipsis(int value) {
    try {
        throw value;
    } catch (...) {
        return 7;
    }
    return 0;
}

extern "C" int cxx_exception_nested(int value) {
    try {
        try {
            throw value;
        } catch (int inner) {
            throw inner + 2;
        }
    } catch (...) {
        return 9;
    }
    return 0;
}

extern "C" int cxx_exception_rethrow(int value) {
    try {
        try {
            throw value;
        } catch (int) {
            throw;
        }
    } catch (int caught) {
        return caught + 3;
    }
    return 0;
}

extern "C" int main() {
    return cxx_exception_path(41) == 42 &&
                   cxx_exception_path(0) == 0 &&
                   cxx_exception_ellipsis(3) == 7 &&
                   cxx_exception_nested(1) == 9 &&
                   cxx_exception_rethrow(5) == 8
               ? 0
               : 1;
}
