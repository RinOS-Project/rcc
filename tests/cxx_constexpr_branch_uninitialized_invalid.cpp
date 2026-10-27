struct UninitializedBodyBranchMember {
    int value;

    constexpr explicit UninitializedBodyBranchMember(int initial) {
        if (initial > 0) {
            this->value = initial;
        }
    }
};

constexpr UninitializedBodyBranchMember
    invalid_uninitialized_body_branch_member{-23};
