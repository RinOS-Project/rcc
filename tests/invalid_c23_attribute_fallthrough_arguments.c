int c23_fallthrough_rejects_arguments(int value) {
    switch (value) {
        case 0:
            [[fallthrough("message")]];
        default:
            return 1;
    }
}
