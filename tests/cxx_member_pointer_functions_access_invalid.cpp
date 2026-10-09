class PrivateMemberFunctionPointerInvalidOwner {
    int apply(int value) {
        return value;
    }
};

int (PrivateMemberFunctionPointerInvalidOwner::*inaccessible_method)(int) =
    &PrivateMemberFunctionPointerInvalidOwner::apply;
