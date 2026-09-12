namespace api {

int transform(int value) {
    return value + 1;
}

namespace nested {

int apply(int value) {
    return ::api::transform(value) + 2;
}

} /* namespace nested */
} /* namespace api */

int call_qualified_namespace(int value) {
    return api::nested::apply(value);
}
