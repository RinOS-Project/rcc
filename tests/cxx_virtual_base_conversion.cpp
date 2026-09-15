class Root {
public:
    int root;
};

class Middle : virtual public Root {
public:
    int middle;
};

class Most : public Middle {
public:
    int most;
};

int main() {
    Most object;
    Middle* middle = &object;
    Root* root = static_cast<Root*>(middle);
    root->root = 7;
    object.middle = 3;
    object.most = 5;
    return root->root == 7 && object.middle == 3 && object.most == 5 ? 0 : 1;
}
