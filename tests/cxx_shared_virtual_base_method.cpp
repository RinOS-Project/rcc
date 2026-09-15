class SharedVirtualBaseMethod {
public:
    int shared;

    int read_shared() { return shared; }
};

class LeftVirtualBranchMethod : virtual public SharedVirtualBaseMethod {
public:
    int left;
};

class RightVirtualBranchMethod : virtual public SharedVirtualBaseMethod {
public:
    int right;
};

class VirtualDiamondMethod : public LeftVirtualBranchMethod,
                             public RightVirtualBranchMethod {
public:
    int own;
};

static_assert(sizeof(VirtualDiamondMethod) ==
              (sizeof(void*) == 4 ? 28 : 56));

int main() {
    VirtualDiamondMethod object;

    object.shared = 7;
    object.left = 2;
    object.right = 3;
    object.own = 5;
    return object.shared == 7 &&
                   object.read_shared() == 7 &&
                   object.left == 2 &&
                   object.right == 3 &&
                   object.own == 5 ? 0 : 1;
}
