struct ReferenceListObject {
    int value;
};

struct ScalarReferenceMember {
    const int& value;
};

struct ClassReferenceMember {
    const ReferenceListObject& value;
};

struct TrackedReferenceTemporary {
    int* destruction_order;
    int id;

    ~TrackedReferenceTemporary() {
        *destruction_order = *destruction_order * 10 + id;
    }
};

struct TrackedReferenceMember {
    const TrackedReferenceTemporary& first;
    const TrackedReferenceTemporary& second;
    int* destruction_order;

    ~TrackedReferenceMember() {
        *destruction_order = *destruction_order * 10 + 3;
    }
};

ScalarReferenceMember global_scalar_member{52};

int global_reference_initializer_calls;

static int global_reference_initializer() {
    ++global_reference_initializer_calls;
    return 53;
}

static ReferenceListObject global_reference_class_initializer() {
    return ReferenceListObject{55};
}

ScalarReferenceMember global_dynamic_scalar_member{
    global_reference_initializer()};

ClassReferenceMember global_class_member{
    ReferenceListObject{54}};

ClassReferenceMember global_dynamic_class_member{
    global_reference_class_initializer()};

static int run_reference_list_initialization() {
    int source = 17;
    int& mutable_lvalue{source};
    const int& const_lvalue{source};
    const int& integer_temporary{42};
    const int& converted_integer{42u};
    const int& copy_list_temporary = {45};
    const float& rounded_float{1.1};
    int&& mutable_rvalue{43};
    const int&& const_rvalue{44};
    const ReferenceListObject& class_temporary{
        ReferenceListObject{46}};
    const ReferenceListObject& class_copy_list = {
        ReferenceListObject{47}};
    ScalarReferenceMember scalar_member{48};
    ScalarReferenceMember scalar_member_copy = {49};
    ClassReferenceMember class_member{ReferenceListObject{50}};
    int side_effect_source = 51;
    ScalarReferenceMember side_effect_member{side_effect_source++};
    int temporary_destruction_order = 0;
    bool temporary_survived_initialization;
    {
        TrackedReferenceMember tracked_member{
            TrackedReferenceTemporary{&temporary_destruction_order, 1},
            TrackedReferenceTemporary{&temporary_destruction_order, 2},
            &temporary_destruction_order};
        temporary_survived_initialization =
            temporary_destruction_order == 0 &&
            tracked_member.first.id == 1 &&
            tracked_member.second.id == 2;
    }

    mutable_lvalue = 18;
    return mutable_lvalue == 18 && const_lvalue == 18 &&
           integer_temporary == 42 && converted_integer == 42 &&
           copy_list_temporary == 45 && rounded_float == 1.1f &&
           mutable_rvalue == 43 &&
           const_rvalue == 44 && class_temporary.value == 46 &&
           class_copy_list.value == 47 && scalar_member.value == 48 &&
           scalar_member_copy.value == 49 &&
           class_member.value.value == 50 &&
           global_scalar_member.value == 52 &&
           global_dynamic_scalar_member.value == 53 &&
           global_class_member.value.value == 54 &&
           global_dynamic_class_member.value.value == 55 &&
           global_reference_initializer_calls == 1 &&
           side_effect_member.value == 51 && side_effect_source == 52 &&
           temporary_survived_initialization &&
           temporary_destruction_order == 213;
}

int main() {
    return run_reference_list_initialization() ? 0 : 1;
}
