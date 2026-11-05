module app;

int api_value() {
    Api api{};
    api.value = dep_value();
    return api.value + 2;
}
