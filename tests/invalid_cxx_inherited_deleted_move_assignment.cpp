struct InheritedDeletedMoveAssignmentBase {
    InheritedDeletedMoveAssignmentBase& operator=(
        const InheritedDeletedMoveAssignmentBase&) {
        return *this;
    }
    InheritedDeletedMoveAssignmentBase& operator=(
        InheritedDeletedMoveAssignmentBase&&) = delete;
};

struct InheritedDeletedMoveAssignmentDerived :
    InheritedDeletedMoveAssignmentBase {
    using InheritedDeletedMoveAssignmentBase::operator=;
};

void assign_from_inherited_deleted_move(
    InheritedDeletedMoveAssignmentDerived& destination,
    InheritedDeletedMoveAssignmentBase&& source) {
    destination = static_cast<InheritedDeletedMoveAssignmentBase&&>(source);
}
