namespace immediate_adl_runtime {
struct tag {};

consteval int target(tag*) {
    return 9;
}

int consume(int (*function)(tag*)) {
    return function(nullptr);
}
}

int invalid_runtime_adl_immediate_pointer_argument() {
    return consume(&immediate_adl_runtime::target);
}
