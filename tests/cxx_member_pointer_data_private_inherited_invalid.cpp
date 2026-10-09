struct MemberPointerPrivateInheritedAccessBase {
private:
    int value;
};

struct MemberPointerPrivateInheritedAccessDerived
    : MemberPointerPrivateInheritedAccessBase {
    friend struct MemberPointerPrivateInheritedAccessReader;
};

struct MemberPointerPrivateInheritedAccessReader {
    static int read(MemberPointerPrivateInheritedAccessDerived& object) {
        return object.value;
    }
};
