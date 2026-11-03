int construction_order = 0;

class RootOrder {
public:
    int value;

    RootOrder(int input) : value(input) {
        construction_order = construction_order * 10 + 1;
    }
};

class MiddleOrder : virtual public RootOrder {
public:
    int middle;

    MiddleOrder(int input) : RootOrder(input), middle(input + 10) {
        construction_order = construction_order * 10 + 2;
    }
};

class MostOrder : public MiddleOrder {
public:
    int most;

    MostOrder(int input)
        : RootOrder(input + 1), MiddleOrder(input), most(input + 2) {
        construction_order = construction_order * 10 + 3;
    }
};

class NestedRootOrder {
public:
    int value;

    NestedRootOrder(int input) : value(input) {
        construction_order = construction_order * 10 + input;
    }
};

class NestedVirtualBranchOrder : virtual public NestedRootOrder {
public:
    int branch;

    NestedVirtualBranchOrder()
        : NestedRootOrder(1), branch(2) {
        construction_order = construction_order * 10 + 3;
    }
};

class NestedNonVirtualBridgeOrder : public NestedVirtualBranchOrder {
public:
    int bridge;

    NestedNonVirtualBridgeOrder()
        : NestedRootOrder(4), NestedVirtualBranchOrder(), bridge(5) {
        construction_order = construction_order * 10 + 6;
    }
};

class NestedMostOrder : public NestedNonVirtualBridgeOrder {
public:
    int most;

    NestedMostOrder()
        : NestedRootOrder(7), NestedNonVirtualBridgeOrder(), most(8) {
        construction_order = construction_order * 10 + 9;
    }
};

int main() {
    MostOrder object(5);
    RootOrder* root = &object;
    if (root->value != 6 || object.middle != 15 || object.most != 7 ||
        construction_order != 123) {
        return 1;
    }

    construction_order = 0;
    NestedMostOrder nested{};
    NestedRootOrder* nested_root = &nested;
    return nested_root->value == 7 && nested.branch == 2 &&
                   nested.bridge == 5 && nested.most == 8 &&
                   construction_order == 7369
        ? 0 : 2;
}
