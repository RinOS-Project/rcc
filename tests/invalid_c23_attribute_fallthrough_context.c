int c23_fallthrough_requires_switch(void) {
    [[fallthrough]];
    return 0;
}

int c23_fallthrough_requires_next_label(int value) {
    switch (value) {
        case 0:
            [[fallthrough]];
            ++value;
        case 1:
            return value;
        default:
            return 0;
    }
}

int c23_fallthrough_requires_same_iteration(int value, int count) {
    switch (value) {
        case 0:
            while (count > 0) {
                --count;
                [[fallthrough]];
            }
        case 1:
            return count;
        default:
            return 0;
    }
}

int c23_fallthrough_must_be_a_block_item(int value) {
    switch (value) {
        case 0:
            if (value) [[fallthrough]];
        case 1:
            return value;
        default:
            return 0;
    }
}
