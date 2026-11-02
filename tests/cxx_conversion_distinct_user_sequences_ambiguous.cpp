struct DistinctUserConversionLeft {};
struct DistinctUserConversionRight {};

struct DistinctUserConversionSource {
    operator DistinctUserConversionLeft() const { return {}; }
    operator DistinctUserConversionRight() const { return {}; }
};

int choose_distinct_user_conversion(DistinctUserConversionLeft) {
    return 1;
}

int choose_distinct_user_conversion(DistinctUserConversionRight) {
    return 2;
}

int main() {
    return choose_distinct_user_conversion(DistinctUserConversionSource{});
}
