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

struct StructuralArrayLeaf {
    int value;
    int extra;
};

struct StructuralNestedArrayKey {
    StructuralArrayLeaf values[2];
};

struct StructuralMatrixKey {
    int values[2][2];
};

struct StructuralClassMatrixElement {
    int value;
    short weight;
};

struct StructuralClassMatrixKey {
    StructuralClassMatrixElement values[2][2];
    int tail;
};

struct StructuralCompactClassMatrixElement {
    unsigned char value;
    unsigned char weight;
};

struct StructuralCompactClassMatrixKey {
    StructuralCompactClassMatrixElement values[2][2];
};

int structural_pointer_array_first(int value) {
    return value + 10;
}

int structural_pointer_array_second(int value) {
    return value * 3;
}

int structural_pointer_array_object_first = 17;
int structural_pointer_array_object_second = 23;

struct StructuralFunctionPointerArrayKey {
    int (*values[2])(int);
};

struct StructuralObjectPointerArrayKey {
    int* values[2];
};

struct StructuralMatrixPointerKey {
    int* values[2][2];
};

struct StructuralMemberPointerArrayOwner {
    int first;
    int second;

    int add_first(int value) const {
        return first + value;
    }

    int add_second(int value) const {
        return second + value;
    }
};

struct StructuralMemberPointerArrayBase {
    int first;
    int second;

    int add_first(int value) const {
        return first + value;
    }

    int add_second(int value) const {
        return second + value;
    }
};

struct StructuralMemberPointerArrayPrefix {
    int prefix;
};

struct StructuralMemberPointerArrayDerived
    : StructuralMemberPointerArrayPrefix,
      StructuralMemberPointerArrayBase {
    int own;
};

struct StructuralDataMemberPointerArrayKey {
    int StructuralMemberPointerArrayOwner::*values[2];
};

struct StructuralDataMemberPointerMatrixKey {
    int StructuralMemberPointerArrayOwner::*values[2][2];
};

struct StructuralFunctionMemberPointerArrayKey {
    int (StructuralMemberPointerArrayOwner::*values[2])(int) const;
};

struct StructuralFunctionMemberPointerMatrixKey {
    int (StructuralMemberPointerArrayOwner::*values[2][2])(int) const;
};

struct StructuralInheritedDataMemberPointerArrayKey {
    int StructuralMemberPointerArrayDerived::*values[2];
};

struct StructuralInheritedFunctionMemberPointerArrayKey {
    int (StructuralMemberPointerArrayDerived::*values[2])(int) const;
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

template <StructuralNestedArrayKey... Keys>
struct StructuralNestedArrayValuePack {
    static constexpr int combined() {
        return ((Keys.values[0].value + Keys.values[0].extra +
                 Keys.values[1].value + Keys.values[1].extra) + ... + 0);
    }
};

template <StructuralMatrixKey... Keys>
struct StructuralMatrixValuePack {
    static constexpr int combined() {
        return ((Keys.values[0][0] + Keys.values[0][1] +
                 Keys.values[1][0] + Keys.values[1][1]) + ... + 0);
    }
};

template <StructuralClassMatrixKey... Keys>
struct StructuralClassMatrixValuePack {
    static constexpr int combined() {
        return ((Keys.values[0][0].value + Keys.values[0][0].weight +
                 Keys.values[0][1].value + Keys.values[0][1].weight +
                 Keys.values[1][0].value + Keys.values[1][0].weight +
                 Keys.values[1][1].value + Keys.values[1][1].weight +
                 Keys.tail) + ... + 0);
    }
};

template <StructuralCompactClassMatrixKey... Keys>
struct StructuralCompactClassMatrixValuePack {
    static constexpr int combined() {
        return ((Keys.values[0][0].value + Keys.values[0][0].weight +
                 Keys.values[0][1].value + Keys.values[0][1].weight +
                 Keys.values[1][0].value + Keys.values[1][0].weight +
                 Keys.values[1][1].value + Keys.values[1][1].weight) + ... + 0);
    }
};

template <StructuralFunctionPointerArrayKey... Keys>
struct StructuralFunctionPointerArrayValuePack {
    static int combined() {
        return ((Keys.values[0](1) + Keys.values[1](2)) + ... + 0);
    }

    static int null_count() {
        return (((Keys.values[0] == nullptr ? 1 : 0) +
                 (Keys.values[1] == nullptr ? 1 : 0)) + ... + 0);
    }
};

template <StructuralObjectPointerArrayKey... Keys>
struct StructuralObjectPointerArrayValuePack {
    static int combined() {
        return ((*Keys.values[0] + *Keys.values[1]) + ... + 0);
    }
};

template <StructuralMatrixPointerKey... Keys>
struct StructuralMatrixPointerValuePack {
    static int combined() {
        return ((*Keys.values[0][0] + *Keys.values[0][1] +
                 *Keys.values[1][0] + *Keys.values[1][1]) + ... + 0);
    }

    static int null_count() {
        return (((Keys.values[0][0] == nullptr ? 1 : 0) +
                 (Keys.values[0][1] == nullptr ? 1 : 0) +
                 (Keys.values[1][0] == nullptr ? 1 : 0) +
                 (Keys.values[1][1] == nullptr ? 1 : 0)) + ... + 0);
    }
};

template <StructuralDataMemberPointerArrayKey... Keys>
struct StructuralDataMemberPointerArrayValuePack {
    static int combined(const StructuralMemberPointerArrayOwner& object) {
        return ((object.*Keys.values[0] + object.*Keys.values[1]) + ... + 0);
    }

    static int null_count() {
        return (((Keys.values[0] == nullptr ? 1 : 0) +
                 (Keys.values[1] == nullptr ? 1 : 0)) + ... + 0);
    }
};

template <StructuralDataMemberPointerMatrixKey... Keys>
struct StructuralDataMemberPointerMatrixValuePack {
    static int combined(const StructuralMemberPointerArrayOwner& object) {
        return ((object.*Keys.values[0][0] + object.*Keys.values[0][1] +
                 object.*Keys.values[1][0] + object.*Keys.values[1][1]) + ... +
                0);
    }

    static int null_count() {
        return (((Keys.values[0][0] == nullptr ? 1 : 0) +
                 (Keys.values[0][1] == nullptr ? 1 : 0) +
                 (Keys.values[1][0] == nullptr ? 1 : 0) +
                 (Keys.values[1][1] == nullptr ? 1 : 0)) + ... + 0);
    }
};

template <StructuralFunctionMemberPointerArrayKey... Keys>
struct StructuralFunctionMemberPointerArrayValuePack {
    static int combined(const StructuralMemberPointerArrayOwner& object) {
        return (((object.*Keys.values[0])(1) +
                 (object.*Keys.values[1])(2)) + ... + 0);
    }

    static int element_count() {
        return 2 * sizeof...(Keys);
    }

    static int null_count() {
        return (((Keys.values[0] == nullptr ? 1 : 0) +
                 (Keys.values[1] == nullptr ? 1 : 0)) + ... + 0);
    }
};

template <StructuralFunctionMemberPointerMatrixKey... Keys>
struct StructuralFunctionMemberPointerMatrixValuePack {
    static int combined(const StructuralMemberPointerArrayOwner& object) {
        return ((((object.*Keys.values[0][0])(1) +
                  (object.*Keys.values[0][1])(2) +
                  (object.*Keys.values[1][0])(3) +
                  (object.*Keys.values[1][1])(4))) + ... + 0);
    }

    static int null_count() {
        return (((Keys.values[0][0] == nullptr ? 1 : 0) +
                 (Keys.values[0][1] == nullptr ? 1 : 0) +
                 (Keys.values[1][0] == nullptr ? 1 : 0) +
                 (Keys.values[1][1] == nullptr ? 1 : 0)) + ... + 0);
    }
};

template <StructuralInheritedDataMemberPointerArrayKey... Keys>
struct StructuralInheritedDataMemberPointerArrayValuePack {
    static int combined(const StructuralMemberPointerArrayDerived& object) {
        return ((object.*Keys.values[0] + object.*Keys.values[1]) + ... + 0);
    }

    static int null_count() {
        return (((Keys.values[0] == nullptr ? 1 : 0) +
                 (Keys.values[1] == nullptr ? 1 : 0)) + ... + 0);
    }
};

template <StructuralInheritedFunctionMemberPointerArrayKey... Keys>
struct StructuralInheritedFunctionMemberPointerArrayValuePack {
    static int combined(const StructuralMemberPointerArrayDerived& object) {
        return (((object.*Keys.values[0])(1) +
                 (object.*Keys.values[1])(2)) + ... + 0);
    }

    static int null_count() {
        return (((Keys.values[0] == nullptr ? 1 : 0) +
                 (Keys.values[1] == nullptr ? 1 : 0)) + ... + 0);
    }
};

int main() {
    StructuralMemberPointerArrayOwner member_pointer_object{13, 29};
    StructuralMemberPointerArrayDerived inherited_member_pointer_object{};
    inherited_member_pointer_object.prefix = 5;
    inherited_member_pointer_object.first = 13;
    inherited_member_pointer_object.second = 29;
    inherited_member_pointer_object.own = 37;
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
               StructuralDoubleArrayKey{{-6.25, 8.75}}>::combined() == 2 &&
           StructuralNestedArrayValuePack<
               StructuralNestedArrayKey{{{5, 50}, {7, 70}}},
               StructuralNestedArrayKey{{{11, 110}, {13, 130}}}
           >::combined() == 396 &&
           StructuralMatrixValuePack<
               StructuralMatrixKey{{{1, 2}, {3, 4}}},
               StructuralMatrixKey{{{5, 6}, {7, 8}}}
           >::combined() == 36 &&
           StructuralClassMatrixValuePack<
               StructuralClassMatrixKey{{{{1, 10}, {2, 20}},
                                         {{3, 30}, {4, 40}}}, 50},
               StructuralClassMatrixKey{{{{5, 60}, {6, 70}},
                                         {{7, 80}, {8, 90}}}, 100}
           >::combined() == 586 &&
           StructuralCompactClassMatrixValuePack<
               StructuralCompactClassMatrixKey{{{{1, 10}, {2, 20}},
                                                {{3, 30}, {4, 40}}}},
               StructuralCompactClassMatrixKey{{{{5, 50}, {6, 60}},
                                                {{7, 70}, {8, 80}}}}
           >::combined() == 396 &&
           StructuralFunctionPointerArrayValuePack<
               StructuralFunctionPointerArrayKey{{
                   &structural_pointer_array_first,
                   &structural_pointer_array_second}},
               StructuralFunctionPointerArrayKey{{
                   &structural_pointer_array_second,
                   &structural_pointer_array_first}}
           >::combined() == 32 &&
           StructuralFunctionPointerArrayValuePack<
               StructuralFunctionPointerArrayKey{{nullptr, nullptr}}
           >::null_count() == 2 &&
           StructuralFunctionPointerArrayValuePack<
               StructuralFunctionPointerArrayKey{{
                   &structural_pointer_array_first, nullptr}}
           >::null_count() == 1 &&
           StructuralObjectPointerArrayValuePack<
               StructuralObjectPointerArrayKey{{
                   &structural_pointer_array_object_first,
                   &structural_pointer_array_object_second}}
           >::combined() == 40 &&
           StructuralMatrixPointerValuePack<
               StructuralMatrixPointerKey{{
                   {&structural_pointer_array_object_first,
                    &structural_pointer_array_object_second},
                   {&structural_pointer_array_object_second,
                    &structural_pointer_array_object_first}}}
           >::combined() == 80 &&
           StructuralMatrixPointerValuePack<
               StructuralMatrixPointerKey{{
                   {nullptr, nullptr}, {nullptr, nullptr}}}
           >::null_count() == 4 &&
           StructuralDataMemberPointerArrayValuePack<
               StructuralDataMemberPointerArrayKey{{
                   &StructuralMemberPointerArrayOwner::first,
                   &StructuralMemberPointerArrayOwner::second}},
               StructuralDataMemberPointerArrayKey{{
                   &StructuralMemberPointerArrayOwner::second,
                   &StructuralMemberPointerArrayOwner::first}}
           >::combined(member_pointer_object) == 84 &&
           StructuralDataMemberPointerArrayValuePack<
               StructuralDataMemberPointerArrayKey{{nullptr, nullptr}}
           >::null_count() == 2 &&
           StructuralDataMemberPointerMatrixValuePack<
               StructuralDataMemberPointerMatrixKey{{
                   {&StructuralMemberPointerArrayOwner::first,
                    &StructuralMemberPointerArrayOwner::second},
                   {&StructuralMemberPointerArrayOwner::second,
                    &StructuralMemberPointerArrayOwner::first}}}
           >::combined(member_pointer_object) == 84 &&
           StructuralDataMemberPointerMatrixValuePack<
               StructuralDataMemberPointerMatrixKey{{
                   {nullptr, nullptr}, {nullptr, nullptr}}}
           >::null_count() == 4 &&
           StructuralFunctionMemberPointerArrayValuePack<
               StructuralFunctionMemberPointerArrayKey{{
                   &StructuralMemberPointerArrayOwner::add_first,
                   &StructuralMemberPointerArrayOwner::add_second}},
               StructuralFunctionMemberPointerArrayKey{{
                   &StructuralMemberPointerArrayOwner::add_second,
                   &StructuralMemberPointerArrayOwner::add_first}}
           >::combined(member_pointer_object) == 90 &&
           StructuralFunctionMemberPointerArrayValuePack<
               StructuralFunctionMemberPointerArrayKey{{nullptr, nullptr}}
           >::element_count() == 2 &&
           StructuralFunctionMemberPointerArrayValuePack<
               StructuralFunctionMemberPointerArrayKey{{nullptr, nullptr}}
           >::null_count() == 2
           && StructuralFunctionMemberPointerMatrixValuePack<
               StructuralFunctionMemberPointerMatrixKey{{
                   {&StructuralMemberPointerArrayOwner::add_first,
                    &StructuralMemberPointerArrayOwner::add_second},
                   {&StructuralMemberPointerArrayOwner::add_first,
                    &StructuralMemberPointerArrayOwner::add_second}}}
           >::combined(member_pointer_object) == 94 &&
           StructuralFunctionMemberPointerMatrixValuePack<
               StructuralFunctionMemberPointerMatrixKey{{
                   {nullptr, nullptr}, {nullptr, nullptr}}}
           >::null_count() == 4
           && StructuralInheritedDataMemberPointerArrayValuePack<
               StructuralInheritedDataMemberPointerArrayKey{{
                   &StructuralMemberPointerArrayBase::first,
                   &StructuralMemberPointerArrayBase::second}}
           >::combined(inherited_member_pointer_object) == 42 &&
           StructuralInheritedDataMemberPointerArrayValuePack<
               StructuralInheritedDataMemberPointerArrayKey{{nullptr, nullptr}}
           >::null_count() == 2 &&
           StructuralInheritedFunctionMemberPointerArrayValuePack<
               StructuralInheritedFunctionMemberPointerArrayKey{{
                   &StructuralMemberPointerArrayBase::add_first,
                   &StructuralMemberPointerArrayBase::add_second}}
           >::combined(inherited_member_pointer_object) == 45 &&
           StructuralInheritedFunctionMemberPointerArrayValuePack<
               StructuralInheritedFunctionMemberPointerArrayKey{{nullptr,
                                                                  nullptr}}
           >::null_count() == 2
        ? 0 : 1;
}
