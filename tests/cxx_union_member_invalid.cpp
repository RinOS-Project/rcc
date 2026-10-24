struct UnionBaseForInvalidTest {
    int base_value;
};

union UnionWithReferenceMember {
    int& reference;
};

union UnionWithVirtualMember {
    virtual void run();
};

union UnionWithBaseClass : UnionBaseForInvalidTest {
    int value;
};

union UnionWithMultipleInitializers {
    int first = 1;
    int second = 2;
};
