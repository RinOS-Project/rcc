class RootConstructed {
public:
    int value;

    RootConstructed(int input) : value(input) {}
};

class DerivedConstructed : virtual public RootConstructed {
public:
    int own;

    DerivedConstructed(int input) : RootConstructed(input), own(input + 1) {}
};

int main() {
    DerivedConstructed object(7);
    RootConstructed* root = &object;
    return root->value == 7 && object.own == 8 ? 0 : 1;
}
