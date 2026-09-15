class SharedVirtualBase {
public:
    int shared;
};

class LeftVirtualBranch : virtual public SharedVirtualBase {
public:
    int left;
};

class RightVirtualBranch : virtual public SharedVirtualBase {
public:
    int right;
};

class VirtualDiamond : public LeftVirtualBranch,
                       public RightVirtualBranch {
public:
    int own;
};

static_assert(sizeof(VirtualDiamond) ==
              (sizeof(void*) == 4 ? 28 : 56));

int main() {
    VirtualDiamond object;

    object.shared = 7;
    object.left = 2;
    object.right = 3;
    object.own = 5;
    return object.shared == 7 &&
                   object.left == 2 &&
                   object.right == 3 &&
                   object.own == 5 ? 0 : 1;
}
