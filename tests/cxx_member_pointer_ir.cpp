struct IrMemberPointerRecord {
    int value;
};

struct IrMemberPointerPrefix {
    int prefix;
};

struct IrMemberPointerBase {
    int inherited;
};

struct IrMemberPointerDerived : IrMemberPointerPrefix,
                                IrMemberPointerBase {
    int tail;
};

struct IrMemberPointerVirtualTag {
    int tag;
};

struct IrMemberPointerMixedDerived : virtual IrMemberPointerVirtualTag,
                                     IrMemberPointerPrefix,
                                     IrMemberPointerBase {
    int tail;
};

struct IrMemberPointerVirtualBase {
    int value;
};

struct IrMemberPointerVirtualMid : virtual IrMemberPointerVirtualBase {
    int mid;
};

struct IrMemberPointerVirtualDerived : IrMemberPointerVirtualMid {
    int tail;
};

extern "C" int ir_member_pointer_read(
    IrMemberPointerRecord* record,
    int IrMemberPointerRecord::*member) {
    return record->*member;
}

extern "C" int ir_member_pointer_write(
    IrMemberPointerRecord* record,
    int IrMemberPointerRecord::*member,
    int value) {
    record->*member = value;
    return record->*member;
}

extern "C" int ir_member_pointer_base_read(
    IrMemberPointerDerived* record,
    int IrMemberPointerBase::*member) {
    return record->*member;
}

extern "C" int ir_member_pointer_converted_read(
    IrMemberPointerDerived* record,
    int IrMemberPointerBase::*member) {
    int IrMemberPointerDerived::*converted = member;
    return record->*converted;
}

extern "C" int ir_member_pointer_unrelated_virtual_read(
    IrMemberPointerMixedDerived* record,
    int IrMemberPointerBase::*member) {
    return record->*member;
}

extern "C" int ir_member_pointer_virtual_owner_conversion_read(
    IrMemberPointerMixedDerived* record,
    int IrMemberPointerBase::*member) {
    int IrMemberPointerMixedDerived::*converted = member;
    return record->*converted;
}

extern "C" int ir_member_pointer_virtual_base_read(
    IrMemberPointerVirtualDerived* record,
    int IrMemberPointerVirtualBase::*member) {
    return record->*member;
}
