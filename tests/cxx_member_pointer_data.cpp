extern "C" {
int cxx_static_reference_events = 0;
int cxx_static_reference_expected_events = 1343;
}

static void record_member_pointer_lifetime_event(int value) {
    cxx_static_reference_events =
        cxx_static_reference_events * 10 + value;
}

struct MemberPointerOwner {
    int value;
    double fraction;

    ~MemberPointerOwner() {
        record_member_pointer_lifetime_event(value);
    }
};

struct MemberPointerAssignmentRecord {
    int value;
};

struct MemberPointerPadding {
    int prefix;
};

struct MemberPointerBase {
    int inherited;
};

struct MemberPointerInheritedPrivateBase {
private:
    int inherited_private;

    friend struct MemberPointerInheritedPrivateReader;
};

struct MemberPointerInheritedPrivateDerived
    : MemberPointerInheritedPrivateBase {};

struct MemberPointerInheritedPrivateReader {
    static int read_private_member() {
        auto member =
            &MemberPointerInheritedPrivateDerived::inherited_private;
        MemberPointerInheritedPrivateDerived object;
        MemberPointerInheritedPrivateBase& base = object;
        base.inherited_private = 43;
        return object.*member;
    }
};

struct MemberPointerProtectedBase {
protected:
    int inherited_protected;
};

struct MemberPointerProtectedDerived : MemberPointerProtectedBase {
    friend struct MemberPointerProtectedReader;
};

struct MemberPointerProtectedReader {
    static int read_protected_member() {
        auto member = &MemberPointerProtectedDerived::inherited_protected;
        MemberPointerProtectedDerived object;
        object.inherited_protected = 47;
        return object.*member;
    }
};

struct MemberPointerHiddenDataBase {
    int hidden;
};

struct MemberPointerHiddenDataDerived : MemberPointerHiddenDataBase {
    int hidden(int input) { return input + 53; }
};

static int invoke_hidden_data_name_method(
    MemberPointerHiddenDataDerived& object, int input) {
    auto method = &MemberPointerHiddenDataDerived::hidden;
    return (object.*method)(input);
}

struct MemberPointerDerived : MemberPointerPadding, MemberPointerBase {
    int tail;
};

struct MemberPointerVirtualTag {
    int tag;
};

struct MemberPointerMixedDerived : virtual MemberPointerVirtualTag,
                                   MemberPointerPadding,
                                   MemberPointerBase {
    int tail;
};

struct MemberPointerVirtualBase {
    int virtual_value;
};

struct MemberPointerVirtualMid : virtual MemberPointerVirtualBase {
    int mid;
};

struct MemberPointerVirtualDerived : MemberPointerVirtualMid {
    int tail;
};

struct MemberPointerPrivateOwner {
private:
    int private_value;

    friend struct MemberPointerPrivateReader;
};

struct MemberPointerPrivateReader {
    static int read_private_member() {
        auto member = &MemberPointerPrivateOwner::private_value;
        MemberPointerPrivateOwner object;
        object.private_value = 29;
        return object.*member;
    }
};

int MemberPointerOwner::*global_value_member = &MemberPointerOwner::value;
double MemberPointerOwner::*global_fraction_member =
    &MemberPointerOwner::fraction;
const int MemberPointerOwner::*global_const_value_member =
    &MemberPointerOwner::value;
int MemberPointerOwner::*global_nullptr_member = nullptr;
int MemberPointerOwner::*global_zero_member = 0;
int MemberPointerOwner::*global_default_member;
int MemberPointerOwner::*global_value_initialized_member{};
int MemberPointerDerived::*global_inherited_member =
    &MemberPointerBase::inherited;
const int MemberPointerDerived::*global_const_inherited_member =
    &MemberPointerBase::inherited;
int MemberPointerBase::*inherited_member_formed_from_derived =
    &MemberPointerDerived::inherited;
int MemberPointerVirtualBase::*virtual_member =
    &MemberPointerVirtualBase::virtual_value;
int MemberPointerVirtualBase::*virtual_member_formed_from_derived =
    &MemberPointerVirtualDerived::virtual_value;

const int& global_member_pointer_reference =
    MemberPointerOwner{3, 1.5}.*global_value_member;

static const int& local_member_pointer_reference() {
    static const int& value =
        MemberPointerOwner{4, 2.5}.*global_value_member;
    return value;
}

static int member_pointer_category(int&) {
    return 1;
}

static int member_pointer_category(int&&) {
    return 2;
}

static int member_pointer_category(const int&) {
    return 3;
}

extern "C" int main() {
    MemberPointerOwner object{1, 2.0};
    MemberPointerAssignmentRecord assignment_destination{1};
    MemberPointerAssignmentRecord assignment_source{41};
    int MemberPointerOwner::*value_member = &MemberPointerOwner::value;
    double MemberPointerOwner::*fraction_member =
        &MemberPointerOwner::fraction;
    MemberPointerOwner* pointer = &object;
    const MemberPointerOwner* const_pointer = &object;
    int MemberPointerOwner::*local_nullptr_member = nullptr;
    int MemberPointerOwner::*local_zero_member = 0;
    int MemberPointerOwner::*local_value_initialized_member{};
    MemberPointerDerived derived;
    derived.prefix = 2;
    derived.inherited = 17;
    derived.tail = 3;
    int MemberPointerBase::*inherited_member =
        &MemberPointerBase::inherited;
    int MemberPointerBase::*base_null_member = nullptr;
    int MemberPointerDerived::*implicitly_converted_member = inherited_member;
    int MemberPointerDerived::*explicitly_converted_member =
        static_cast<int MemberPointerDerived::*>(inherited_member);
    int MemberPointerBase::*round_trip_member =
        static_cast<int MemberPointerBase::*>(explicitly_converted_member);
    int MemberPointerDerived::*converted_null_member =
        static_cast<int MemberPointerDerived::*>(base_null_member);
    MemberPointerDerived* derived_pointer = &derived;
    MemberPointerMixedDerived mixed_derived;
    mixed_derived.tag = 5;
    mixed_derived.prefix = 6;
    mixed_derived.inherited = 23;
    mixed_derived.tail = 7;
    int MemberPointerMixedDerived::*mixed_inherited_member =
        inherited_member;
    MemberPointerVirtualDerived virtual_derived;
    virtual_derived.virtual_value = 31;
    virtual_derived.mid = 8;
    virtual_derived.tail = 9;

    if (object.*value_member != 1 ||
        member_pointer_category((object.*value_member)) != 1) {
        return 1;
    }
    static_cast<MemberPointerAssignmentRecord&&>(assignment_destination) =
        assignment_source;
    if (assignment_destination.value != 41) return 19;
    if (member_pointer_category(const_pointer->*value_member) != 3 ||
        member_pointer_category(object.*global_const_value_member) != 3) {
        return 18;
    }
    (object.*value_member) = 11;
    if (&(object.*value_member) != &object.value || object.value != 11) {
        return 2;
    }
    (pointer->*value_member) += 1;
    if (object.value != 12 || pointer->*value_member != 12) return 3;

    if (member_pointer_category(
            static_cast<MemberPointerOwner&&>(object).*value_member) != 2) {
        return 4;
    }
    (object.*value_member) = 13;
    if (object.value != 13) return 5;

    if (derived.*inherited_member != 17) return 10;
    if (derived_pointer->*inherited_member != 17) return 12;
    if (derived.*implicitly_converted_member != 17 ||
        derived.*explicitly_converted_member != 17 ||
        derived.*global_inherited_member != 17 ||
        derived.*global_const_inherited_member != 17 ||
        derived.*inherited_member_formed_from_derived != 17) {
        return 13;
    }
    if (derived.*round_trip_member != 17) return 14;
    if (mixed_derived.*inherited_member != 23 ||
        mixed_derived.*mixed_inherited_member != 23) {
        return 15;
    }
    if (virtual_derived.*virtual_member != 31 ||
        (&virtual_derived)->*virtual_member != 31 ||
        virtual_derived.*virtual_member_formed_from_derived != 31) {
        return 16;
    }
    (virtual_derived.*virtual_member) = 37;
    if (virtual_derived.virtual_value != 37 ||
        MemberPointerPrivateReader::read_private_member() != 29) {
        return 17;
    }
    if (MemberPointerInheritedPrivateReader::read_private_member() != 43 ||
        MemberPointerProtectedReader::read_protected_member() != 47) {
        return 20;
    }
    MemberPointerHiddenDataDerived hidden_data_object;
    if (invoke_hidden_data_name_method(hidden_data_object, 4) != 57) {
        return 21;
    }
    (derived.*inherited_member) = 19;
    if (derived.inherited != 19 || derived.tail != 3) return 11;

    (object.*fraction_member) = 6.25;
    if ((object.*fraction_member) != 6.25 ||
        (object.*global_fraction_member) != 6.25) {
        return 6;
    }
    if (global_member_pointer_reference != 3) return 7;
    if (global_nullptr_member != nullptr || global_zero_member != 0 ||
        global_default_member != nullptr ||
        global_value_initialized_member != nullptr ||
        converted_null_member != nullptr ||
        local_nullptr_member != nullptr || local_zero_member != 0 ||
        local_value_initialized_member != nullptr) {
        return 9;
    }

    const int& first_local = local_member_pointer_reference();
    const int& second_local = local_member_pointer_reference();
    if (first_local != 4 || second_local != 4 ||
        &first_local != &second_local || cxx_static_reference_events != 0) {
        return 8;
    }
    return 0;
}
