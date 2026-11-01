struct AggregateValue {
    int value;
};

struct NestedValue {
    unsigned char byte;
};

struct NestedAggregate {
    NestedValue nested;
};

struct ConstructedValue {
    int value;
    ConstructedValue(int source) : value(source) {}
};

int main() {
    int source = 1;
    int source_array[1] = {1};
    AggregateValue from_float{1.5};
    NestedAggregate nested_out_of_range{{256}};
    ConstructedValue constructor_from_float{1.5};
    unsigned char out_of_range{256};
    unsigned char array_out_of_range[1]{256};
    float inexact_integer{16777217};
    double runtime_fraction = 1.5;
    float runtime_narrow{runtime_fraction};
    float floating_overflow{1e100};
    bool nonzero_integer{2};
    bool from_pointer{&source};
    bool from_array{source_array};
    return from_float.value + nested_out_of_range.nested.byte +
           sizeof(constructor_from_float) + out_of_range +
           array_out_of_range[0] + (int)inexact_integer +
           (int)runtime_narrow + (int)floating_overflow +
           nonzero_integer + from_pointer + from_array;
}
