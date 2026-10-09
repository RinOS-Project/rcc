struct VirtualMemberPointerBase {
    int apply(int value) { return value; }
};

struct VirtualMemberPointerDerived : virtual VirtualMemberPointerBase {};

int (VirtualMemberPointerBase::*base_method)(int) =
    &VirtualMemberPointerBase::apply;

int (VirtualMemberPointerDerived::*invalid_virtual_owner_method)(int) =
    static_cast<int (VirtualMemberPointerDerived::*)(int)>(base_method);
