struct DesignatedRecord {
    int first;
    int second;
    int third;
};

struct DesignatedNested {
    DesignatedRecord record;
    int marker;
};

extern "C" int probe_cxx_designated_initializer(void) {
    DesignatedRecord value{.first = 3, .third = 4};
    DesignatedNested nested{.record = {.first = 5, .third = 6},
                            .marker = 7};
    return value.first * 100 + value.second * 10 + value.third +
           nested.record.first + nested.record.third + nested.marker;
}
