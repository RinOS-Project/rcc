static thread_local int verified_global_tls_initializer_calls;
static thread_local int verified_local_tls_initializer_calls;
static thread_local int verified_dynamic_tls_aggregate_calls;
static thread_local int verified_default_member_initializer_calls;
extern "C" {
extern int verified_dynamic_tls_aggregate_destructor_count;
}

static int verified_global_tls_next_value(int seed) {
    return seed + ++verified_global_tls_initializer_calls;
}

static int verified_local_tls_next_value(int seed) {
    return seed + ++verified_local_tls_initializer_calls;
}

static int verified_dynamic_tls_default_member_value(int seed) {
    return seed + ++verified_default_member_initializer_calls;
}

struct VerifiedDynamicTlsAggregate {
    volatile int value;
    volatile int marker;
    volatile int default_initialized =
        verified_dynamic_tls_default_member_value(90);
    volatile int explicit_after_default;
    volatile int zero_initialized;

    ~VerifiedDynamicTlsAggregate() {
        __atomic_add_fetch(
            &verified_dynamic_tls_aggregate_destructor_count, 1,
            __ATOMIC_SEQ_CST);
    }
};

static thread_local VerifiedDynamicTlsAggregate verified_global_tls_aggregate = {
    .value = verified_global_tls_next_value(40),
    .marker = verified_global_tls_next_value(70),
    .explicit_after_default = 27
};

static VerifiedDynamicTlsAggregate* verified_dynamic_tls_aggregate_instance() {
    static thread_local VerifiedDynamicTlsAggregate aggregate = {
        .value = verified_local_tls_next_value(40),
        .marker = verified_local_tls_next_value(70),
        .explicit_after_default = 19
    };
    return &aggregate;
}

extern "C" int verified_dynamic_tls_aggregate_probe(
    int expected_value, int next_value) {
    VerifiedDynamicTlsAggregate* aggregate =
        verified_dynamic_tls_aggregate_instance();
    VerifiedDynamicTlsAggregate* global_aggregate =
        &verified_global_tls_aggregate;
    int call = verified_dynamic_tls_aggregate_calls++;
    if (verified_local_tls_initializer_calls != 2 ||
        verified_global_tls_initializer_calls != 2) {
        return 1;
    }
    if (verified_default_member_initializer_calls != 2) return 4;
    if (aggregate->default_initialized ==
            global_aggregate->default_initialized ||
        aggregate->default_initialized +
                global_aggregate->default_initialized != 183) {
        return 5;
    }
    if (aggregate->zero_initialized != 0 ||
        global_aggregate->zero_initialized != 0) {
        return 6;
    }
    if (aggregate->explicit_after_default != 19 ||
        global_aggregate->explicit_after_default != 27) {
        return 7;
    }
    if (call == 0) {
        if (aggregate->value != 41 || aggregate->marker != 72 ||
            global_aggregate->value != 41 ||
            global_aggregate->marker != 72) return 2;
    } else if (aggregate->value != expected_value ||
               aggregate->marker != expected_value + 1 ||
               global_aggregate->value != expected_value + 1000 ||
               global_aggregate->marker != expected_value + 1001) {
        return 3;
    }
    aggregate->value = next_value;
    aggregate->marker = next_value + 1;
    global_aggregate->value = next_value + 1000;
    global_aggregate->marker = next_value + 1001;
    return 0;
}

extern "C" volatile int* verified_dynamic_tls_aggregate_address(void) {
    return &verified_dynamic_tls_aggregate_instance()->value;
}

extern "C" volatile int* verified_global_tls_aggregate_address(void) {
    return &verified_global_tls_aggregate.value;
}
