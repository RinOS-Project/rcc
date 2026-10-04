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

int read_virtual_root(Most& object) {
    Root& root = object;
    root.root = 11;
    return root.root;
}

int main() {
    Most object;
    Middle* middle = &object;
    Root* root = static_cast<Root*>(middle);
    root->root = 7;
    object.middle = 3;
    object.most = 5;
    return read_virtual_root(object) == 11 && root->root == 11 &&
                   object.middle == 3 && object.most == 5 ? 0 : 1;
}
