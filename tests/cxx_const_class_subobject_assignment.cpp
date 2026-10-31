struct ConstQualifiedAssignment {
    ConstQualifiedAssignment& operator=(
        const ConstQualifiedAssignment&) const;
};

struct ConstClassMemberAssignable {
    const ConstQualifiedAssignment value;
};

ConstClassMemberAssignable& assign_const_class_member(
    ConstClassMemberAssignable& target,
    const ConstClassMemberAssignable& source) {
    return target = source;
}
