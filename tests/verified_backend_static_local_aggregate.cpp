static int first_initializer_calls;
static int second_initializer_calls;
static int default_member_first_calls;
static int default_member_second_calls;
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

static int make_static_aggregate_default_first() {
    ++default_member_first_calls;
    return 31;
}

static int make_static_aggregate_default_second() {
    ++default_member_second_calls;
    return 41;
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

struct StaticLocalDefaultMemberAggregate {
    int first = make_static_aggregate_default_first();
    int second = make_static_aggregate_default_second();

    ~StaticLocalDefaultMemberAggregate() {
        ++verified_static_local_aggregate_destructions;
    }
};

static int read_static_local_default_member_aggregate() {
    static StaticLocalDefaultMemberAggregate value{};
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
    if (read_static_local_default_member_aggregate() != 72 ||
        default_member_first_calls != 1 || default_member_second_calls != 1) {
        return 7;
    }
    if (read_static_local_default_member_aggregate() != 72 ||
        default_member_first_calls != 1 || default_member_second_calls != 1 ||
        verified_static_local_aggregate_destructions != 0) {
        return 8;
    }
    return 0;
}

extern "C" int verified_static_local_aggregate_destructor_count() {
    return verified_static_local_aggregate_destructions;
}
