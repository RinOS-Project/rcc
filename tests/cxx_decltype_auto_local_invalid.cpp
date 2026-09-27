extern "C" int decltype_auto_local_invalid(void) {
    decltype(auto) braced = {1};
    return braced;
}
