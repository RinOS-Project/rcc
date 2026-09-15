extern "C" int cxx_exception_invalid_catch_order(int value) {
    try {
        throw value;
    } catch (...) {
        return 1;
    } catch (int) {
        return 2;
    }
}
