const int& narrowed_reference{1.5};
int& nonconst_reference_to_temporary{42};
const int& multiple_reference_clauses{1, 2};

struct ScalarReferenceMember {
    const int& value;
};

struct MutableReferenceMember {
    int& value;
};

ScalarReferenceMember narrowed_member{1.5};
MutableReferenceMember invalid_member{42};

static void take_const_reference(const int&) {}

int main() {
    take_const_reference({1.5});
    return narrowed_reference + nonconst_reference_to_temporary +
           multiple_reference_clauses + narrowed_member.value +
           invalid_member.value;
}
