struct LifetimeProbe {
    int* destructions;

    ~LifetimeProbe() {
        ++*destructions;
    }
};

struct ReferenceAggregate {
    const LifetimeProbe& value;
    int observed_before_cleanup;
};

static int observe_before_cleanup(const int* destructions) {
    return *destructions == 0 ? 73 : -1;
}

static int global_destructions = 0;
static ReferenceAggregate global_aggregate(
    LifetimeProbe{&global_destructions},
    observe_before_cleanup(&global_destructions));

static ReferenceAggregate& function_static_aggregate(int* destructions) {
    static ReferenceAggregate aggregate(
        LifetimeProbe{destructions}, observe_before_cleanup(destructions));
    return aggregate;
}

int main() {
    if (global_destructions != 1) {
        return 1;
    }
    if (global_aggregate.observed_before_cleanup != 73) return 2;

    int static_destructions = 0;
    ReferenceAggregate& static_aggregate =
        function_static_aggregate(&static_destructions);
    int static_reentry_destructions = 0;
    if (static_destructions != 1) return 30;
    if (static_aggregate.observed_before_cleanup != 73) return 31;
    if (function_static_aggregate(&static_reentry_destructions)
            .observed_before_cleanup != 73) return 32;
    if (static_reentry_destructions != 0) return 33;

    int destructions = 0;
    {
        ReferenceAggregate aggregate(
            LifetimeProbe{&destructions},
            observe_before_cleanup(&destructions));
        if (destructions != 1 || aggregate.observed_before_cleanup != 73) {
            return 4;
        }
    }
    return destructions == 1 ? 0 : 5;
}
