struct EqualityLeaf {
    int value;
    bool operator==(const EqualityLeaf&) const = default;
};

struct EqualityPrefix {
    int prefix_value;
    bool operator==(const EqualityPrefix&) const = default;
};

int base_comparisons;

struct EqualityBase {
    int base_value;
    bool operator==(const EqualityBase& other) const {
        ++base_comparisons;
        return base_value == other.base_value;
    }
};

struct EqualityRecord : EqualityPrefix, EqualityBase {
    int id;
    EqualityLeaf leaf;
    int values[2][2];
    bool operator==(const EqualityRecord&) const = default;
};

struct EqualityVirtualBase {
    int virtual_base_value;
    bool operator==(const EqualityVirtualBase&) const = default;
};

struct EqualityVirtual : virtual EqualityVirtualBase {
    int value;
    EqualityVirtual() {}
    bool operator==(const EqualityVirtual&) const = default;
};

int main() {
    EqualityRecord first{{2}, {5}, 7, {11}, {{13, 17}, {19, 23}}};
    EqualityRecord same{{2}, {5}, 7, {11}, {{13, 17}, {19, 23}}};
    EqualityRecord different_prefix{{3}, {5}, 7, {11}, {{13, 17}, {19, 23}}};
    EqualityRecord different_base{{2}, {6}, 7, {11}, {{13, 17}, {19, 23}}};
    EqualityRecord different_leaf{{2}, {5}, 7, {12}, {{13, 17}, {19, 23}}};
    EqualityRecord different_array{{2}, {5}, 7, {11}, {{13, 17}, {19, 24}}};
    EqualityVirtual virtual_first;
    EqualityVirtual virtual_same;
    EqualityVirtual virtual_base_different;
    EqualityVirtual virtual_member_different;
    virtual_first.virtual_base_value = 29;
    virtual_first.value = 31;
    virtual_same.virtual_base_value = 29;
    virtual_same.value = 31;
    virtual_base_different.virtual_base_value = 30;
    virtual_base_different.value = 31;
    virtual_member_different.virtual_base_value = 29;
    virtual_member_different.value = 32;
    EqualityPrefix& prefix_left = first;
    const EqualityPrefix& prefix_right = same;
    EqualityBase& base_left = first;
    const EqualityBase& base_right = same;
    if (!(prefix_left == prefix_right)) return 10;
    if (!(base_left == base_right)) return 11;
    base_comparisons = 0;
    if (!(first == same)) return 1;
    if (base_comparisons != 1) return 2;
    if (first == different_prefix) return 3;
    if (first == different_base) return 4;
    if (first == different_leaf) return 5;
    if (first == different_array) return 6;
    if (!(virtual_first == virtual_same)) return 7;
    if (virtual_first == virtual_base_different) return 8;
    if (virtual_first == virtual_member_different) return 9;
    return 0;
}
