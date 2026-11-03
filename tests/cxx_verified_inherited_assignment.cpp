struct VerifiedInheritedAssignmentBase {
    int value;
    VerifiedInheritedAssignmentBase& operator=(
        const VerifiedInheritedAssignmentBase&) = default;
};

struct VerifiedInheritedAssignmentMiddle : VerifiedInheritedAssignmentBase {
    using VerifiedInheritedAssignmentBase::operator=;
    int middle;
};

struct VerifiedInheritedAssignmentDerived :
    VerifiedInheritedAssignmentMiddle {
    using VerifiedInheritedAssignmentMiddle::operator=;
    int tail;
};

void verified_inherited_assignment_copy(
    VerifiedInheritedAssignmentDerived& destination,
    const VerifiedInheritedAssignmentBase& source) {
    destination = source;
}
