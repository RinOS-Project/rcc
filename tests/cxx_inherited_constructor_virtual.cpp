static int inherited_virtual_events[16];
static int inherited_virtual_event_count;

static void record_inherited_virtual_event(int value) noexcept {
    inherited_virtual_events[inherited_virtual_event_count++] = value;
}

struct InheritedVirtualTemplateBase {
    template <typename T>
    explicit InheritedVirtualTemplateBase(T value) noexcept : value_(value) {
        record_inherited_virtual_event(1);
    }

    ~InheritedVirtualTemplateBase() {
        record_inherited_virtual_event(-1);
    }

    int value_;
};

struct InheritedVirtualLayer : virtual InheritedVirtualTemplateBase {
    using InheritedVirtualTemplateBase::InheritedVirtualTemplateBase;
    int layer_value = 2;
};

struct InheritedVirtualLeaf : InheritedVirtualLayer {
    using InheritedVirtualLayer::InheritedVirtualLayer;
    int leaf_value = 3;
};

static_assert(__is_constructible(InheritedVirtualLayer, int),
              "direct virtual inherited constructor template");
static_assert(__is_constructible(InheritedVirtualLeaf, int),
              "linear inherited constructor-template chain through a virtual base");
static_assert(__is_nothrow_constructible(InheritedVirtualLayer, int),
              "direct virtual inherited constructor preserves noexcept");
static_assert(__is_nothrow_constructible(InheritedVirtualLeaf, int),
              "virtual-base inherited constructor chain preserves noexcept");

int main() {
    static const int expected[] = {1, -1, 1, -1};
    inherited_virtual_event_count = 0;
    {
        InheritedVirtualLayer object(17);
        if (object.value_ != 17 || object.layer_value != 2 ||
            inherited_virtual_event_count != 1 ||
            inherited_virtual_events[0] != 1) {
            return 1;
        }
    }
    {
        InheritedVirtualLeaf object(29);
        if (object.value_ != 29 || object.layer_value != 2 ||
            object.leaf_value != 3 || inherited_virtual_event_count != 3 ||
            inherited_virtual_events[2] != 1) {
            return 4;
        }
    }
    if (inherited_virtual_event_count !=
        (int)(sizeof(expected) / sizeof(expected[0]))) {
        return 2;
    }
    for (int index = 0; index < inherited_virtual_event_count; ++index) {
        if (inherited_virtual_events[index] != expected[index]) return 3;
    }
    return 0;
}
