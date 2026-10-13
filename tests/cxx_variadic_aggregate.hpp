struct VariadicMixedAggregate {
    long long integer;
    double floating;
};

struct VariadicMemoryAggregate {
    long long first;
    long long second;
    long long third;
};

struct VariadicAlignedMemoryAggregate {
    alignas(16) long long first;
    long long second;
    long long third;
    long long fourth;
};

struct VariadicDoublePair {
    double first;
    double second;
};

int read_variadic_aggregate_registers(
    int marker0, int marker1, int marker2, int marker3, int marker4,
    double fp0, double fp1, double fp2, double fp3, double fp4, double fp5,
    double fp6, ...);

int read_variadic_aggregate_gp_exhausted(
    int marker0, int marker1, int marker2, int marker3, int marker4,
    int marker5, ...);

int read_variadic_aligned_aggregate(
    int marker0, int marker1, int marker2, int marker3, int marker4,
    int marker5, ...);

int read_variadic_sse_aggregate(
    double fp0, double fp1, double fp2, double fp3, double fp4, ...);
