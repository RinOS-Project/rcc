static int first_initializer_calls;
static int second_initializer_calls;
extern "C" {
#ifdef RCC_VERIFIED_STATIC_LOCAL_AGGREGATE_HOST
extern int verified_static_local_aggregate_destructions;
#else
int verified_static_local_aggregate_destructions;
#endif
}

static int make_static_aggregate_first() {
    ++first_initializer_calls;
    if (first_initializer_calls == 1) throw 29;
    return 17;
}

static int make_static_aggregate_second() {
    ++second_initializer_calls;
    return 23;
}

struct StaticLocalAggregate {
    int first;
    int second;

    ~StaticLocalAggregate() {
        ++verified_static_local_aggregate_destructions;
    }
};

static int read_static_local_aggregate() {
    static StaticLocalAggregate value{
        make_static_aggregate_first(), make_static_aggregate_second()};
    return value.first + value.second;
}

int main() {
    try {
        (void)read_static_local_aggregate();
        return 1;
    } catch (int error) {
        if (error != 29) return 2;
    }
    if (first_initializer_calls != 1 || second_initializer_calls != 0) {
        return 3;
    }
    if (read_static_local_aggregate() != 40 ||
        first_initializer_calls != 2 || second_initializer_calls != 1) {
        return 4;
    }
    if (read_static_local_aggregate() != 40 ||
        first_initializer_calls != 2 || second_initializer_calls != 1) {
        return 5;
    }
    if (verified_static_local_aggregate_destructions != 0) return 6;
    return 0;
}

extern "C" int verified_static_local_aggregate_destructor_count() {
    return verified_static_local_aggregate_destructions;
}
