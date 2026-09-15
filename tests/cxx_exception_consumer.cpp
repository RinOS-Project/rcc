struct SharedLibraryException {
    int value;
};

extern "C" void cxx_exception_provider_throw();

extern "C" int main() {
    try {
        cxx_exception_provider_throw();
    } catch (SharedLibraryException caught) {
        return caught.value == 41 ? 0 : 1;
    } catch (...) {
        return 2;
    }
    return 3;
}
