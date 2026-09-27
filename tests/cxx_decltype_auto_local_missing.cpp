extern "C" int decltype_auto_local_missing(void) {
    decltype(auto) missing;
    return 0;
}
