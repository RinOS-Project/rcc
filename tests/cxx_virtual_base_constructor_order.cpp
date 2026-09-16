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

int main() {
    MostOrder object(5);
    RootOrder* root = &object;
    return root->value == 6 && object.middle == 15 && object.most == 7 &&
           construction_order == 123
        ? 0 : 1;
}
