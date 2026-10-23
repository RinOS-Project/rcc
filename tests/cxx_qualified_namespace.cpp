namespace api {

typedef int result_type;

int transform(int value) {
    return value + 1;
}

namespace nested {

int apply(int value) {
    return ::api::transform(value) + 2;
}

} /* namespace nested */
} /* namespace api */

namespace api_short = api;

api::result_type call_qualified_namespace_type(api_short::result_type value) {
    api::result_type direct = value;
    api_short::result_type through_alias = direct + 1;
    return through_alias;
}

int call_qualified_namespace(int value) {
    return api::nested::apply(value);
}
