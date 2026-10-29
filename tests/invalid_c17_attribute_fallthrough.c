int c17_fallthrough_requires_c23(int value) {
    switch (value) {
        case 0:
            [[fallthrough]];
        default:
            return 1;
    }
}
