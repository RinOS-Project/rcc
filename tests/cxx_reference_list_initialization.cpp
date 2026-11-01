struct ReferenceListObject {
    int value;
};

struct ScalarReferenceMember {
    const int& value;
};

struct ClassReferenceMember {
    const ReferenceListObject& value;
};

ScalarReferenceMember global_scalar_member{52};

int global_reference_initializer_calls;

static int global_reference_initializer() {
    ++global_reference_initializer_calls;
    return 53;
}

ScalarReferenceMember global_dynamic_scalar_member{
    global_reference_initializer()};

ClassReferenceMember global_class_member{
    ReferenceListObject{54}};

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
           global_reference_initializer_calls == 1 &&
           side_effect_member.value == 51 && side_effect_source == 52;
}

int main() {
    return run_reference_list_initialization() ? 0 : 1;
}
