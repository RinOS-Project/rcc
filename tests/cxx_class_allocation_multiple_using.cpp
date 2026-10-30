using allocation_size_type = __SIZE_TYPE__;

struct LeftUsingAllocationBase {
    void* operator new(allocation_size_type size);
};

struct RightUsingAllocationBase {
    void* operator new(allocation_size_type size);
};

struct MultipleUsingAllocation : LeftUsingAllocationBase,
                                 RightUsingAllocationBase {
    using LeftUsingAllocationBase::operator new;
    using RightUsingAllocationBase::operator new;
};

MultipleUsingAllocation* allocate_multiple_using_operators() {
    return new MultipleUsingAllocation;
}
