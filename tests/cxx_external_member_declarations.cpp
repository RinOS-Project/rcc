struct ExternalMethodApi {
    int read() const;
    operator int() const;
};

#if defined(TEST_EXTERNAL_MEMBER_DEFINITIONS)
int ExternalMethodApi::read() const {
    return 42;
}

ExternalMethodApi::operator int() const {
    return 73;
}
#else
int call_external_member(const ExternalMethodApi& api) {
    return api.read();
}

int call_external_conversion(const ExternalMethodApi& api) {
    return api;
}

#if defined(TEST_EXTERNAL_MEMBER_MAIN)
int main() {
    ExternalMethodApi api{};
    return call_external_member(api) == 42 &&
        call_external_conversion(api) == 73 ? 0 : 1;
}
#endif
#endif
