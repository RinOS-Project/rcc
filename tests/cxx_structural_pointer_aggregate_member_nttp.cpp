struct StructuralPointerAggregatePair {
    int first;
    int second;
};

struct StructuralPointerAggregateEnvelope {
    char tag;
    StructuralPointerAggregatePair pair;
    int samples[3];
    StructuralPointerAggregatePair groups[2];
};

StructuralPointerAggregateEnvelope structural_pointer_aggregate_envelope = {
    'R', {13, 29}, {31, 47, 59}, {{61, 71}, {83, 97}}
};

struct StructuralPointerAggregateKey {
    int* pointer;
};

template <StructuralPointerAggregateKey Value>
struct StructuralPointerAggregateOwner {
    static int get() {
        return *Value.pointer;
    }

    static int* address() {
        return Value.pointer;
    }
};

int main() {
    int nested_member = StructuralPointerAggregateOwner<
        StructuralPointerAggregateKey{
            &structural_pointer_aggregate_envelope.pair.second}>::get();
    int array_element = StructuralPointerAggregateOwner<
        StructuralPointerAggregateKey{
            &structural_pointer_aggregate_envelope.samples[1]}>::get();
    int array_element_two = StructuralPointerAggregateOwner<
        StructuralPointerAggregateKey{
            &structural_pointer_aggregate_envelope.samples[2]}>::address() ==
        &structural_pointer_aggregate_envelope.samples[2];
    int nested_array_member = StructuralPointerAggregateOwner<
        StructuralPointerAggregateKey{
            &structural_pointer_aggregate_envelope.groups[1].second}>::get();
    return nested_member == 29 && array_element == 47 && array_element_two &&
        nested_array_member == 97 ? 0 : 1;
}
