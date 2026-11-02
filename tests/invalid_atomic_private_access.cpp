#include <atomic>

struct AtomicPrivateCopyConstructorValue {
    AtomicPrivateCopyConstructorValue() = default;

private:
    AtomicPrivateCopyConstructorValue(
        const AtomicPrivateCopyConstructorValue&) = default;
};

struct AtomicProtectedCopyConstructorValue {
    AtomicProtectedCopyConstructorValue() = default;

protected:
    AtomicProtectedCopyConstructorValue(
        const AtomicProtectedCopyConstructorValue&) = default;
};

struct AtomicPrivateMoveConstructorValue {
    AtomicPrivateMoveConstructorValue() = default;
    AtomicPrivateMoveConstructorValue(
        const AtomicPrivateMoveConstructorValue&) = default;

private:
    AtomicPrivateMoveConstructorValue(
        AtomicPrivateMoveConstructorValue&&) = default;
};

struct AtomicProtectedMoveConstructorValue {
    AtomicProtectedMoveConstructorValue() = default;
    AtomicProtectedMoveConstructorValue(
        const AtomicProtectedMoveConstructorValue&) = default;

protected:
    AtomicProtectedMoveConstructorValue(
        AtomicProtectedMoveConstructorValue&&) = default;
};

struct AtomicPrivateCopyAssignmentValue {
    AtomicPrivateCopyAssignmentValue() = default;

private:
    AtomicPrivateCopyAssignmentValue& operator=(
        const AtomicPrivateCopyAssignmentValue&) = default;
};

struct AtomicPrivateMoveAssignmentValue {
    AtomicPrivateMoveAssignmentValue() = default;
    AtomicPrivateMoveAssignmentValue& operator=(
        const AtomicPrivateMoveAssignmentValue&) = default;

private:
    AtomicPrivateMoveAssignmentValue& operator=(
        AtomicPrivateMoveAssignmentValue&&) = default;
};

struct AtomicProtectedMoveAssignmentValue {
    AtomicProtectedMoveAssignmentValue() = default;
    AtomicProtectedMoveAssignmentValue& operator=(
        const AtomicProtectedMoveAssignmentValue&) = default;

protected:
    AtomicProtectedMoveAssignmentValue& operator=(
        AtomicProtectedMoveAssignmentValue&&) = default;
};

struct AtomicProtectedCopyAssignmentValue {
    AtomicProtectedCopyAssignmentValue() = default;

protected:
    AtomicProtectedCopyAssignmentValue& operator=(
        const AtomicProtectedCopyAssignmentValue&) = default;
};

struct AtomicPrivateSubobjectCopyConstructor {
    AtomicPrivateSubobjectCopyConstructor() = default;
    AtomicPrivateSubobjectCopyConstructor(
        AtomicPrivateSubobjectCopyConstructor&&) = default;
    AtomicPrivateSubobjectCopyConstructor& operator=(
        const AtomicPrivateSubobjectCopyConstructor&) = default;
    AtomicPrivateSubobjectCopyConstructor& operator=(
        AtomicPrivateSubobjectCopyConstructor&&) = default;

private:
    AtomicPrivateSubobjectCopyConstructor(
        const AtomicPrivateSubobjectCopyConstructor&) = default;
};

struct AtomicPrivateSubobjectCopyConstructorOwner {
    AtomicPrivateSubobjectCopyConstructor value;
};

struct AtomicPrivateSubobjectCopyAssignment {
    AtomicPrivateSubobjectCopyAssignment() = default;
    AtomicPrivateSubobjectCopyAssignment(
        const AtomicPrivateSubobjectCopyAssignment&) = default;
    AtomicPrivateSubobjectCopyAssignment(
        AtomicPrivateSubobjectCopyAssignment&&) = default;
    AtomicPrivateSubobjectCopyAssignment& operator=(
        AtomicPrivateSubobjectCopyAssignment&&) = default;

private:
    AtomicPrivateSubobjectCopyAssignment& operator=(
        const AtomicPrivateSubobjectCopyAssignment&) = default;
};

struct AtomicPrivateSubobjectCopyAssignmentOwner {
    AtomicPrivateSubobjectCopyAssignment value;
};

struct AtomicPrivateCopyBase {
    AtomicPrivateCopyBase() = default;
    AtomicPrivateCopyBase(AtomicPrivateCopyBase&&) = default;
    AtomicPrivateCopyBase& operator=(AtomicPrivateCopyBase&&) = default;

private:
    AtomicPrivateCopyBase(const AtomicPrivateCopyBase&) = default;
    AtomicPrivateCopyBase& operator=(const AtomicPrivateCopyBase&) = default;
};

struct AtomicPrivateBaseOwner : AtomicPrivateCopyBase {};

void instantiate_atomic_with_inaccessible_copy_move_members() {
    std::atomic<AtomicPrivateCopyConstructorValue> private_constructor;
    std::atomic<AtomicProtectedCopyConstructorValue> protected_constructor;
    std::atomic<AtomicPrivateMoveConstructorValue> private_move_constructor;
    std::atomic<AtomicProtectedMoveConstructorValue> protected_move_constructor;
    std::atomic<AtomicPrivateCopyAssignmentValue> private_assignment;
    std::atomic<AtomicProtectedCopyAssignmentValue> protected_assignment;
    std::atomic<AtomicPrivateSubobjectCopyConstructorOwner>
        private_subobject_copy_constructor;
    std::atomic<AtomicPrivateSubobjectCopyAssignmentOwner>
        private_subobject_copy_assignment;
    std::atomic<AtomicPrivateBaseOwner> private_base;
    std::atomic<AtomicPrivateMoveAssignmentValue> private_move_assignment;
    std::atomic<AtomicProtectedMoveAssignmentValue> protected_move_assignment;
    (void)private_constructor;
    (void)protected_constructor;
    (void)private_move_constructor;
    (void)protected_move_constructor;
    (void)private_assignment;
    (void)protected_assignment;
    (void)private_subobject_copy_constructor;
    (void)private_subobject_copy_assignment;
    (void)private_base;
    (void)private_move_assignment;
    (void)protected_move_assignment;
}
