static int construction_count;

struct ArrayOrderElement {
    int order;

    ArrayOrderElement() {
        construction_count = construction_count + 1;
        order = construction_count;
    }
};

struct ArrayOrderBase {
    explicit ArrayOrderBase(int value) : base_value(value) {}

    int base_value;
};

template <typename T>
int array_order_probe(T marker_value) {
    struct OrdinaryArrayOwner {
        T marker;
        ArrayOrderElement elements[3];

        explicit OrdinaryArrayOwner(T value) : marker(value) {}
    };

    struct InheritedArrayOwner : ArrayOrderBase {
        using ArrayOrderBase::ArrayOrderBase;

        ArrayOrderElement elements[3];
    };

    construction_count = 0;
    OrdinaryArrayOwner ordinary(marker_value);
    if (construction_count != 3 || ordinary.marker != marker_value ||
        ordinary.elements[0].order != 1 ||
        ordinary.elements[1].order != 2 ||
        ordinary.elements[2].order != 3) {
        return 1;
    }

    construction_count = 0;
    InheritedArrayOwner via_base(29);
    if (construction_count != 3) return 2;
    if (via_base.base_value != 29) return 3;
    if (via_base.elements[0].order != 1 ||
        via_base.elements[1].order != 2 ||
        via_base.elements[2].order != 3) {
        return 4;
    }
    return 0;
}

int main() {
    int result = array_order_probe(17);
    if (result != 0) return result;
    result = array_order_probe(29LL);
    if (result != 0) return result + 10;
    return 0;
}
