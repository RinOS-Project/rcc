using allocation_size_type = __SIZE_TYPE__;

struct CommonDiamondAllocationBase {
    void* operator new(allocation_size_type size);
};

struct LeftRepeatedAllocationBase : CommonDiamondAllocationBase {};
struct RightRepeatedAllocationBase : CommonDiamondAllocationBase {};
struct RepeatedAllocationDiamond : LeftRepeatedAllocationBase,
                                   RightRepeatedAllocationBase {};

struct LeftVirtualAllocationBase : virtual CommonDiamondAllocationBase {};
struct RightVirtualAllocationBase : virtual CommonDiamondAllocationBase {};
struct VirtualAllocationDiamond : LeftVirtualAllocationBase,
                                  RightVirtualAllocationBase {};

RepeatedAllocationDiamond* allocate_repeated_base_operator() {
    return new RepeatedAllocationDiamond;
}

VirtualAllocationDiamond* allocate_virtual_base_operator() {
    return new VirtualAllocationDiamond;
}
