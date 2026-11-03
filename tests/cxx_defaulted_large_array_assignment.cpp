static unsigned* watched_array_destination;
static double* watched_weight_destination;
static unsigned** watched_reference_destination;
static unsigned expected_first_value;
static unsigned expected_last_value;
static double expected_first_weight;
static double expected_last_weight;
static unsigned* expected_first_reference;
static unsigned* expected_last_reference;
static unsigned member_order_checks;

struct LargeArrayAssignmentMarker {
    int value;
    int assignments;

    LargeArrayAssignmentMarker& operator=(
        const LargeArrayAssignmentMarker& source) {
        value = source.value;
        ++assignments;
        if (watched_array_destination &&
            watched_array_destination[0] == expected_first_value &&
            watched_array_destination[2047] == expected_last_value &&
            watched_weight_destination[0] == expected_first_weight &&
            watched_weight_destination[95] == expected_last_weight &&
            watched_reference_destination[0] == expected_first_reference &&
            watched_reference_destination[127] == expected_last_reference) {
            ++member_order_checks;
        }
        return *this;
    }
};

struct LargeScalarArrayAssignment {
    unsigned values[2048];
    unsigned rows[3][256];
    double weights[96];
    unsigned* references[128];
    LargeArrayAssignmentMarker marker;

    LargeScalarArrayAssignment& operator=(
        const LargeScalarArrayAssignment&) = default;
    LargeScalarArrayAssignment& operator=(
        LargeScalarArrayAssignment&&) = default;
};

int main() {
    LargeScalarArrayAssignment source;
    LargeScalarArrayAssignment destination;
    for (unsigned index = 0; index < 2048; ++index) {
        source.values[index] = index * 37u + 11u;
        destination.values[index] = 0u;
    }
    for (unsigned row = 0; row < 3; ++row) {
        for (unsigned column = 0; column < 256; ++column) {
            source.rows[row][column] = row * 1009u + column * 13u;
            destination.rows[row][column] = 0u;
        }
    }
    for (unsigned index = 0; index < 96; ++index) {
        source.weights[index] = static_cast<double>(index) + 0.125;
        destination.weights[index] = -1.0;
    }
    for (unsigned index = 0; index < 128; ++index) {
        source.references[index] = &source.values[index * 16u];
        destination.references[index] = 0;
    }
    source.marker.value = 73;
    destination.marker.value = -1;
    destination.marker.assignments = 0;
    watched_array_destination = destination.values;
    watched_weight_destination = destination.weights;
    watched_reference_destination = destination.references;
    expected_first_value = source.values[0];
    expected_last_value = source.values[2047];
    expected_first_weight = source.weights[0];
    expected_last_weight = source.weights[95];
    expected_first_reference = source.references[0];
    expected_last_reference = source.references[127];
    member_order_checks = 0;

    destination = source;
    if (destination.marker.value != 73 ||
        destination.marker.assignments != 1 || member_order_checks != 1) {
        return 1;
    }
    for (unsigned index = 0; index < 2048; ++index) {
        if (destination.values[index] != index * 37u + 11u) {
            return 2;
        }
    }
    for (unsigned row = 0; row < 3; ++row) {
        for (unsigned column = 0; column < 256; ++column) {
            if (destination.rows[row][column] !=
                row * 1009u + column * 13u) {
                return 3;
            }
        }
    }
    for (unsigned index = 0; index < 96; ++index) {
        if (destination.weights[index] !=
            static_cast<double>(index) + 0.125) {
            return 4;
        }
    }
    for (unsigned index = 0; index < 128; ++index) {
        if (destination.references[index] !=
            &source.values[index * 16u]) {
            return 5;
        }
    }

    destination = destination;
    if (destination.marker.assignments != 2) return 6;
    for (unsigned index = 0; index < 2048; ++index) {
        if (destination.values[index] != index * 37u + 11u) {
            return 7;
        }
    }
    for (unsigned row = 0; row < 3; ++row) {
        for (unsigned column = 0; column < 256; ++column) {
            if (destination.rows[row][column] !=
                row * 1009u + column * 13u) {
                return 8;
            }
        }
    }

    LargeScalarArrayAssignment moved;
    moved.marker.assignments = 0;
    watched_array_destination = moved.values;
    watched_weight_destination = moved.weights;
    watched_reference_destination = moved.references;
    member_order_checks = 0;
    moved = static_cast<LargeScalarArrayAssignment&&>(source);
    if (moved.marker.value != 73 || moved.marker.assignments != 1 ||
        member_order_checks != 1) {
        return 9;
    }
    for (unsigned index = 0; index < 2048; ++index) {
        if (moved.values[index] != index * 37u + 11u) return 10;
    }
    for (unsigned row = 0; row < 3; ++row) {
        for (unsigned column = 0; column < 256; ++column) {
            if (moved.rows[row][column] !=
                row * 1009u + column * 13u) {
                return 11;
            }
        }
    }
    for (unsigned index = 0; index < 96; ++index) {
        if (moved.weights[index] != static_cast<double>(index) + 0.125) {
            return 12;
        }
    }
    for (unsigned index = 0; index < 128; ++index) {
        if (moved.references[index] != &source.values[index * 16u]) {
            return 13;
        }
    }
    return 0;
}
