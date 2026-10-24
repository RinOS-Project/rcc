static int virtual_diamond_events[4];
static int virtual_diamond_event_count;

struct VirtualDiamondOrigin {
    template <typename T>
    explicit VirtualDiamondOrigin(T value) noexcept : value_(value) {
        virtual_diamond_events[virtual_diamond_event_count++] = 1;
    }

    ~VirtualDiamondOrigin() {
        virtual_diamond_events[virtual_diamond_event_count++] = -1;
    }

    int value_;
};

struct VirtualDiamondBase : VirtualDiamondOrigin {
    using VirtualDiamondOrigin::VirtualDiamondOrigin;
    int base_value = 5;
};

struct VirtualDiamondLeft : virtual VirtualDiamondBase {
    using VirtualDiamondBase::VirtualDiamondBase;
    int left_value = 2;
};

struct VirtualDiamondRight : virtual VirtualDiamondBase {
    using VirtualDiamondBase::VirtualDiamondBase;
    int right_value = 3;
};

struct VirtualDiamond : VirtualDiamondLeft, VirtualDiamondRight {
    using VirtualDiamondLeft::VirtualDiamondLeft;
    using VirtualDiamondRight::VirtualDiamondRight;
    int diamond_value = 4;
};

static_assert(__is_constructible(VirtualDiamond, int),
              "constructibility through a shared virtual-base constructor");
static_assert(__is_nothrow_constructible(VirtualDiamond, int),
              "nothrow constructibility follows a shared virtual-base constructor");

int main() {
    virtual_diamond_event_count = 0;
    {
        VirtualDiamond object(17);
        if (object.value_ != 17 || object.base_value != 5 ||
            object.left_value != 2 ||
            object.right_value != 3 || object.diamond_value != 4 ||
            virtual_diamond_event_count != 1 ||
            virtual_diamond_events[0] != 1) {
            return 1;
        }
    }
    return virtual_diamond_event_count == 2 &&
                   virtual_diamond_events[1] == -1
        ? 0 : 2;
}
