namespace api {
inline namespace v1 {
struct Versioned {
    int value;
};

int make_value(int value) {
    return value + 1;
}
}
}

int main() {
    api::Versioned value = {7};
    return api::make_value(4) == 5 &&
                   api::v1::make_value(8) == 9 &&
                   value.value == 7
               ? 0
               : 1;
}
