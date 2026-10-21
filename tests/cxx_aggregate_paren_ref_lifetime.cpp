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

int main() {
    if (global_destructions != 1 ||
        global_aggregate.observed_before_cleanup != 73) {
        return 1;
    }

    int destructions = 0;
    {
        ReferenceAggregate aggregate(
            LifetimeProbe{&destructions},
            observe_before_cleanup(&destructions));
        if (destructions != 1 || aggregate.observed_before_cleanup != 73) {
            return 2;
        }
    }
    return destructions == 1 ? 0 : 3;
}
