struct SharedLibraryException {
    int value;
};

extern "C" void cxx_exception_provider_throw() {
    SharedLibraryException value{41};
    throw value;
}
