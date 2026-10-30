using allocation_size_type = __SIZE_TYPE__;

struct AllocationUsingBase {
    void* operator new(allocation_size_type size);
    void operator delete(void* pointer) noexcept;
};

struct PrivateAllocationUsing : public AllocationUsingBase {
private:
    using AllocationUsingBase::operator new;

public:
    using AllocationUsingBase::operator delete;
};

PrivateAllocationUsing* allocate_private_operator() {
    return new PrivateAllocationUsing;
}
