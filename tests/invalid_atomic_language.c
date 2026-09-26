struct atomic_record {
    int value;
};

_Atomic(struct atomic_record) invalid_aggregate_atomic;
_Atomic(const int) invalid_qualified_atomic;

int main(void) {
    return invalid_aggregate_atomic.value + invalid_qualified_atomic;
}
