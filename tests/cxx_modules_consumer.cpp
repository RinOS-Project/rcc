import app;

int main() {
    Api api{};
    api.value = dep_value();
    return api_value() == 42 && api.value == 40 ? 0 : 1;
}
