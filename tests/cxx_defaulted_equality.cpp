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

struct EqualityHiddenFriend : EqualityPrefix {
    int values[2];
    friend bool operator==(const EqualityHiddenFriend&,
                           const EqualityHiddenFriend&) = default;
};

struct EqualityValueFriend {
    int value;
    friend bool operator==(EqualityValueFriend,
                           EqualityValueFriend) = default;
};

struct EqualityDirectNotEqual {
    int value;
    bool operator==(const EqualityDirectNotEqual&) const = default;
    bool operator!=(const EqualityDirectNotEqual& other) const {
        return value == other.value;
    }
};

struct EqualityReversedFriend {
    int value;
    friend bool operator==(int expected,
                           const EqualityReversedFriend& actual) {
        return expected == actual.value;
    }
};

struct EqualityInheritedBase {
    int prefix;
    bool operator==(const EqualityInheritedBase& other) const {
        return prefix == other.prefix;
    }
};

struct EqualityInheritedReverse : EqualityInheritedBase {
    int value;
    friend bool operator==(int expected,
                           const EqualityInheritedReverse& actual) {
        return expected == actual.value;
    }
};

struct EqualityNonviableNotEqual {
    int value;
    friend bool operator==(const EqualityNonviableNotEqual&,
                           const EqualityNonviableNotEqual&) = default;
    friend bool operator!=(const EqualityNonviableNotEqual&, int) = delete;
};

struct EqualityNonviableMemberNotEqual {
    int value;
    bool operator==(const EqualityNonviableMemberNotEqual&) const = default;
    bool operator!=(int) const = delete;
};

struct EqualityDirectFriendNotEqual {
    int value;
    friend bool operator==(const EqualityDirectFriendNotEqual&,
                           const EqualityDirectFriendNotEqual&) = default;
    friend bool operator!=(const EqualityDirectFriendNotEqual& left,
                           const EqualityDirectFriendNotEqual& right) {
        return left.value == right.value;
    }
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
    EqualityHiddenFriend hidden_first{{37}, {41, 43}};
    EqualityHiddenFriend hidden_same{{37}, {41, 43}};
    EqualityHiddenFriend hidden_base_different{{38}, {41, 43}};
    EqualityHiddenFriend hidden_array_different{{37}, {41, 44}};
    EqualityValueFriend value_friend_first{47};
    EqualityValueFriend value_friend_same{47};
    EqualityValueFriend value_friend_different{48};
    EqualityDirectNotEqual direct_not_equal_first{53};
    EqualityDirectNotEqual direct_not_equal_same{53};
    EqualityReversedFriend reversed_friend{59};
    EqualityInheritedReverse inherited_reverse{{67}, 71};
    EqualityNonviableNotEqual nonviable_not_equal_first{61};
    EqualityNonviableNotEqual nonviable_not_equal_same{61};
    EqualityNonviableNotEqual nonviable_not_equal_different{62};
    EqualityNonviableMemberNotEqual nonviable_member_first{73};
    EqualityNonviableMemberNotEqual nonviable_member_same{73};
    EqualityNonviableMemberNotEqual nonviable_member_different{74};
    EqualityDirectFriendNotEqual direct_friend_not_equal_first{79};
    EqualityDirectFriendNotEqual direct_friend_not_equal_same{79};
    if (hidden_first.values[1] != 43 ||
        hidden_array_different.values[1] != 44) return 17;
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
    if (first != same) return 18;
    if (!(first != different_array)) return 19;
    if (!(virtual_first == virtual_same)) return 7;
    if (virtual_first == virtual_base_different) return 8;
    if (virtual_first == virtual_member_different) return 9;
    if (!(hidden_first == hidden_same)) return 12;
    if (hidden_first == hidden_base_different) return 13;
    if (hidden_first == hidden_array_different) return 14;
    if (hidden_first != hidden_same) return 20;
    if (!(hidden_first != hidden_array_different)) return 21;
    if (!(value_friend_first == value_friend_same)) return 15;
    if (value_friend_first == value_friend_different) return 16;
    if (!(direct_not_equal_first != direct_not_equal_same)) return 22;
    if (reversed_friend != 59) return 23;
    if (!(reversed_friend != 60)) return 24;
    if (!(reversed_friend == 59)) return 27;
    if (reversed_friend == 60) return 28;
    if (!(inherited_reverse == 71)) return 29;
    if (inherited_reverse == 72) return 30;
    if (inherited_reverse != 71) return 31;
    if (!(inherited_reverse != 72)) return 32;
    if (nonviable_not_equal_first != nonviable_not_equal_same) return 25;
    if (!(nonviable_not_equal_first != nonviable_not_equal_different)) {
        return 26;
    }
    if (nonviable_member_first != nonviable_member_same) return 33;
    if (!(nonviable_member_first != nonviable_member_different)) return 34;
    if (!(direct_friend_not_equal_first != direct_friend_not_equal_same)) {
        return 35;
    }
    return 0;
}
