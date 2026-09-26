namespace api::v2 {
struct Value {
    int value;
};

int plus_one(int value) {
    return value + 1;
}
}

int main() {
    api::v2::Value value = {7};
    return api::v2::plus_one(value.value) == 8 ? 0 : 1;
}
