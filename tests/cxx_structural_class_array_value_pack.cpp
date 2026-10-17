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

struct StructuralFloatArrayKey {
    float values[2];
};

struct StructuralDoubleArrayKey {
    double values[2];
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

template <StructuralFloatArrayKey... Keys>
struct StructuralFloatArrayValuePack {
    static constexpr int combined() {
        return ((((Keys.values[0] == 1.25f && Keys.values[1] == -2.5f) ||
                  (Keys.values[0] == 3.75f && Keys.values[1] == -4.125f))
                     ? 1
                     : 0) + ... + 0);
    }
};

template <StructuralDoubleArrayKey... Keys>
struct StructuralDoubleArrayValuePack {
    static constexpr int combined() {
        return ((((Keys.values[0] == 3.125 && Keys.values[1] == -4.5) ||
                  (Keys.values[0] == -6.25 && Keys.values[1] == 8.75))
                     ? 1
                     : 0) + ... + 0);
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
           >::combined() == 17 &&
           StructuralFloatArrayValuePack<
               StructuralFloatArrayKey{{1.25f, -2.5f}},
               StructuralFloatArrayKey{{3.75f, -4.125f}}>::combined() == 2 &&
           StructuralDoubleArrayValuePack<
               StructuralDoubleArrayKey{{3.125, -4.5}},
               StructuralDoubleArrayKey{{-6.25, 8.75}}>::combined() == 2
        ? 0 : 1;
}
