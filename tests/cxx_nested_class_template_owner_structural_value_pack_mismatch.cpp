struct NestedPackLengthKey {
    int value;
};

enum class NestedPackLengthCode : int {
    one = 1,
    two = 2,
};

struct NestedPackLengthEnumKey {
    NestedPackLengthCode value;
};

template <NestedPackLengthKey... LeftValues>
struct NestedPackLengthOwner {
    template <NestedPackLengthEnumKey... RightValues>
    struct Zip {
        template <typename... Elements>
        int combined() const {
            return ((LeftValues.value +
                     static_cast<int>(RightValues.value) +
                     static_cast<int>(sizeof(Elements))) + ... + 0);
        }
    };
};

int main() {
    NestedPackLengthOwner<
        NestedPackLengthKey{1}, NestedPackLengthKey{2}>::Zip<
            NestedPackLengthEnumKey{NestedPackLengthCode::one},
            NestedPackLengthEnumKey{NestedPackLengthCode::two}>
        zip;
    return zip.combined<char, int, long>();
}
