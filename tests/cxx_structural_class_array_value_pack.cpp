struct StructuralArrayKey {
    int values[2];
};

struct StructuralArrayRecordKey {
    int values[2];
    int tail;
};

enum StructuralArrayEnumValue {
    StructuralArrayEnumTwo = 2,
    StructuralArrayEnumThree = 3,
    StructuralArrayEnumFive = 5,
    StructuralArrayEnumSeven = 7,
};

struct StructuralEnumArrayKey {
    StructuralArrayEnumValue values[2];
};

template <StructuralArrayKey... Keys>
struct StructuralArrayValuePack {
    static constexpr int combined() {
        return ((Keys.values[0] + Keys.values[1]) + ... + 0);
    }
};

template <StructuralArrayRecordKey... Keys>
struct StructuralArrayRecordValuePack {
    static constexpr int combined() {
        return ((Keys.values[0] + Keys.values[1] + Keys.tail) + ... + 0);
    }
};

template <StructuralEnumArrayKey... Keys>
struct StructuralEnumArrayValuePack {
    static constexpr int combined() {
        return ((Keys.values[0] + Keys.values[1]) + ... + 0);
    }
};

int main() {
    return StructuralArrayValuePack<
               StructuralArrayKey{{2, 3}},
               StructuralArrayKey{{5, 7}}>::combined() == 17 &&
           StructuralArrayRecordValuePack<
               StructuralArrayRecordKey{{2, 3}, 4},
               StructuralArrayRecordKey{{5, 7}, 9}>::combined() == 30 &&
           StructuralEnumArrayValuePack<
               StructuralEnumArrayKey{{
                   StructuralArrayEnumTwo, StructuralArrayEnumThree}},
               StructuralEnumArrayKey{{
                   StructuralArrayEnumFive, StructuralArrayEnumSeven}}
           >::combined() == 17
        ? 0 : 1;
}
