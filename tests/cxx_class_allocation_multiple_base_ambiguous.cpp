using allocation_size_type = __SIZE_TYPE__;

struct LeftAllocationBase {
    void* operator new(allocation_size_type size);
    void* operator new[](allocation_size_type size);
};

struct RightAllocationBase {
    void* operator new(allocation_size_type size);
    void* operator new[](allocation_size_type size);
};

struct AmbiguousMultipleBaseAllocation : LeftAllocationBase,
                                         RightAllocationBase {};

AmbiguousMultipleBaseAllocation* allocate_ambiguous_multiple_base() {
    return new AmbiguousMultipleBaseAllocation;
}

AmbiguousMultipleBaseAllocation* allocate_ambiguous_multiple_base_array() {
    return new AmbiguousMultipleBaseAllocation[2];
}
