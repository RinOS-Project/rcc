extern "C" int cxx_exception_path(int value) {
    try {
        if (value) throw value;
    } catch (int caught) {
        return caught;
    }
    return 0;
}
