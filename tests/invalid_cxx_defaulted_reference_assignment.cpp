struct ReferenceMemberDefaultedAssignment {
    int& value;
    ReferenceMemberDefaultedAssignment& operator=(
        const ReferenceMemberDefaultedAssignment&) = default;
};

void assign_reference_member_defaulted_value(
    ReferenceMemberDefaultedAssignment& destination,
    const ReferenceMemberDefaultedAssignment& source) {
    destination = source;
}
