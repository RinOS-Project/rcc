unsigned bare_unsigned_identity(unsigned value)
{
    return value;
}

signed bare_signed_identity(signed value)
{
    return value;
}

int switch_basic(int value)
{
    int result = 1;
    switch (value) {
        default:
            result = 99;
            break;
        case -3:
            result = 10;
            break;
        case 0:
            result = 20;
        case 1:
            result += 3;
            break;
        case 2:
        case 3:
            result = 33;
            break;
    }
    return result;
}

unsigned long long switch_unsigned_wide(unsigned long long value)
{
    switch (value) {
        case 0x100000000ULL:
            return 11ULL;
        case 0x8000000000000000ULL:
            return 22ULL;
        case 0xffffffffffffffffULL:
            return 33ULL;
        default:
            return 44ULL;
    }
}

long long switch_signed_wide(long long value)
{
    switch (value) {
        case -1LL:
            return -11LL;
        case -0x100000000LL:
            return -22LL;
        case 0x100000000LL:
            return 33LL;
        default:
            return 44LL;
    }
}

static unsigned switch_read_once(unsigned* value, int* calls)
{
    ++*calls;
    return *value;
}

int switch_evaluates_once(unsigned* value, int* calls)
{
    switch (switch_read_once(value, calls)) {
        case 7u:
            return 70;
        default:
            return 90;
    }
}

int switch_nested(int outer, int inner)
{
    int result = 0;
    switch (outer) {
        case 1:
            result = 10;
            switch (inner) {
                case 2:
                    result += 2;
                    break;
                default:
                    result += 5;
                    break;
            }
            result += 100;
            break;
        default:
            result = -1;
            break;
    }
    return result;
}

int switch_inside_loop(int limit)
{
    int index;
    int result = 0;
    for (index = 0; index < limit; ++index) {
        switch (index) {
            case 1:
                continue;
            case 3:
                result += 30;
                break;
            default:
                result += index;
                break;
        }
        result += 100;
    }
    return result;
}

int switch_signed_char(signed char value)
{
    switch (value) {
        case -1:
            return 1;
        case 127:
            return 2;
        default:
            return 3;
    }
}

int switch_unsigned_char(unsigned char value)
{
    switch (value) {
        case 255:
            return 1;
        case 127:
            return 2;
        default:
            return 3;
    }
}
