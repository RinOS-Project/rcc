static int increment(int value) { return value + 1; }

static int call_pointer(int (*function)(int), int value) {
    return function(value);
}

extern "C" {
    int probe_function_pointer(void) {
        int (*function)(int) = increment;
        return call_pointer(function, 41);
    }

    int probe_range_for(void) {
        int values[3] = { 1, 2, 3 };
        int total = 0;
        for (auto value : values) {
            total += value;
        }
        return total;
    }

    int probe_range_for_explicit(void) {
        int values[2] = { 4, 5 };
        int total = 0;
        for (const int value : values) {
            total += value;
        }
        return total;
    }

    int probe_range_for_reference(void) {
        int values[3] = { 1, 2, 3 };
        for (auto& value : values) {
            value += 10;
        }
        return values[0] + values[1] + values[2];
    }

    int probe_range_for_const_reference(void) {
        int values[3] = { 6, 7, 8 };
        int total = 0;
        for (const auto& value : values) {
            total += value;
        }
        return total;
    }
}
