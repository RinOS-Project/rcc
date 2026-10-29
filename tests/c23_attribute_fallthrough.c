#ifdef __RCC__
#if !defined(__has_c_attribute)
#error "C23 __has_c_attribute is unavailable"
#endif
#if __has_c_attribute(fallthrough) != 202311L
#error "fallthrough attribute version is incorrect"
#endif
#else
#if !__has_c_attribute(fallthrough)
#error "fallthrough attribute is unavailable"
#endif
#endif

int c23_fallthrough_basic(int value) {
    int result = 0;
    switch (value) {
        case 0:
            result += 1;
            [[fallthrough]];
        case 1:
            result += 2;
            break;
        default:
            break;
    }
    return result;
}

int c23_fallthrough_iteration(int value, int count) {
    int result = 0;
    for (int i = 0; i < count; ++i) {
        switch (value) {
            case 3:
                ++result;
                [[fallthrough]];
            default:
                result += 2;
        }
    }
    return result;
}

int c23_fallthrough_label_chain(int value) {
    int result = 0;
    switch (value) {
        case 4:
        case 5: [[fallthrough]];
        case 6:
            result = 6;
            break;
        default:
            break;
    }
    return result;
}

int main(void) {
    if (c23_fallthrough_basic(0) != 3 ||
        c23_fallthrough_basic(1) != 2 ||
        c23_fallthrough_basic(2) != 0) {
        return 1;
    }
    if (c23_fallthrough_iteration(3, 2) != 6 ||
        c23_fallthrough_iteration(4, 2) != 4) {
        return 2;
    }
    if (c23_fallthrough_label_chain(4) != 6 ||
        c23_fallthrough_label_chain(5) != 6) {
        return 3;
    }
    return 0;
}
