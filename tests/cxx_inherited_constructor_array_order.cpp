static int construction_count;

template <typename T>
int array_order_probe(T marker_value) {
    struct ArrayOrderElement {
        int order;

        ArrayOrderElement() {
            construction_count = construction_count + 1;
            order = construction_count;
        }
    };

    struct OrdinaryArrayOwner {
        T marker;
        ArrayOrderElement elements[3];

        explicit OrdinaryArrayOwner(T value) : marker(value) {}
    };

    struct ArrayOrderBase {
        explicit ArrayOrderBase(T value) : base_value(value) {}

        T base_value;
    };

    struct InheritedArrayOwner : ArrayOrderBase {
        using ArrayOrderBase::ArrayOrderBase;

        ArrayOrderElement elements[3];
    };

    OrdinaryArrayOwner ordinary(marker_value);
    if (construction_count != 3 || ordinary.marker != marker_value ||
        ordinary.elements[0].order != 1 ||
        ordinary.elements[1].order != 2 ||
        ordinary.elements[2].order != 3) {
        return 1;
    }

    construction_count = 0;
    InheritedArrayOwner via_base(marker_value);
    if (construction_count != 3 || via_base.base_value != marker_value ||
        via_base.elements[0].order != 1 ||
        via_base.elements[1].order != 2 ||
        via_base.elements[2].order != 3) {
        return 2;
    }
    return 0;
}

int main() {
    if (array_order_probe(17) != 0) return 1;
    if (array_order_probe(29LL) != 0) return 2;
    return 0;
}
