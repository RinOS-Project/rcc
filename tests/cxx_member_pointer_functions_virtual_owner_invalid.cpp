struct VirtualMemberPointerBase {
    int value;
    int apply(int value) { return value; }
};

struct VirtualMemberPointerDerived : virtual VirtualMemberPointerBase {};

int (VirtualMemberPointerBase::*base_method)(int) =
    &VirtualMemberPointerBase::apply;

int (VirtualMemberPointerDerived::*invalid_virtual_owner_method)(int) =
    static_cast<int (VirtualMemberPointerDerived::*)(int)>(base_method);

int VirtualMemberPointerBase::*base_value =
    &VirtualMemberPointerBase::value;

int VirtualMemberPointerDerived::*invalid_virtual_owner_data = base_value;

struct VirtualMemberPointerIndirectRoot {
    int value;
    int apply(int argument) { return value + argument; }
};

struct VirtualMemberPointerIndirectIntermediate
    : VirtualMemberPointerIndirectRoot {};

struct VirtualMemberPointerIndirectDerived
    : virtual VirtualMemberPointerIndirectIntermediate {};

int VirtualMemberPointerIndirectRoot::*indirect_root_value =
    &VirtualMemberPointerIndirectRoot::value;

int VirtualMemberPointerIndirectDerived::*invalid_virtual_chain_data =
    static_cast<int VirtualMemberPointerIndirectDerived::*>(
        indirect_root_value);

int (VirtualMemberPointerIndirectRoot::*indirect_root_method)(int) =
    &VirtualMemberPointerIndirectRoot::apply;

int (VirtualMemberPointerIndirectDerived::*invalid_virtual_chain_method)(int) =
    static_cast<int (VirtualMemberPointerIndirectDerived::*)(int)>(
        indirect_root_method);
