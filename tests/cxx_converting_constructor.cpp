class Converting {
public:
    Converting(int value) : value_(value) {}
    explicit Converting(long value)
        : value_(static_cast<int>(value) + 100) {}

    int value() const {
        return value_;
    }

private:
    int value_;
};

struct SliceBase {
    int value;
};

struct SliceDerived : SliceBase {
    int extra;
};

struct AggregateCastTarget {
    int first;
    int value_initialized;
    int default_initialized = 19;
};

AggregateCastTarget global_aggregate_cast_value =
    static_cast<AggregateCastTarget>(29);

class ClassCastSource;

int class_cast_target_destructor_count = 0;

class ClassCastTarget {
public:
    explicit ClassCastTarget(int value) : value_(value) {}
    explicit ClassCastTarget(const ClassCastSource& source)
        : value_(300) {
        (void)source;
    }
    ~ClassCastTarget() {
        ++class_cast_target_destructor_count;
    }

    int value() const {
        return value_;
    }

private:
    int value_;
};

class ClassCastSource {
public:
    explicit ClassCastSource(int value) : value_(value) {}

    explicit operator ClassCastTarget() const {
        return ClassCastTarget(value_ + 200);
    }

private:
    int value_;
};

class OperatorOnlySource {
public:
    explicit OperatorOnlySource(int value) : value_(value) {}

    explicit operator ClassCastTarget() const {
        return ClassCastTarget(value_ + 200);
    }

private:
    int value_;
};

OperatorOnlySource global_operator_only_source(5);
ClassCastTarget global_class_cast_target =
    static_cast<ClassCastTarget>(global_operator_only_source);

#if defined(TEST_INVALID_CLASS_TARGET_STATIC_CAST)
struct NoConversionTarget {};

void reject_invalid_class_target_static_cast() {
    (void)static_cast<NoConversionTarget>(1);
}
#endif

static int consume(Converting object) {
    return object.value();
}

    Converting global_value = 2;

extern "C" int cxx_converting_constructor(void) {
    Converting local_value = 4;
    Converting direct_value(5);
    Converting copied_value = direct_value;
    Converting explicit_cast_value = static_cast<Converting>(8L);
    SliceDerived derived_value;
    derived_value.value = 11;
    derived_value.extra = 12;
    SliceBase sliced_value = static_cast<SliceBase>(derived_value);
    AggregateCastTarget aggregate_cast_value =
        static_cast<AggregateCastTarget>(23);
    Converting* heap_value = new Converting(6);
    int cast_value = int(8);
    int brace_cast_value = int{9};
    int empty_cast_value = int{};
    int result = global_value.value() + local_value.value() +
                 direct_value.value() + copied_value.value() +
                 explicit_cast_value.value() +
                 sliced_value.value + aggregate_cast_value.first +
                 aggregate_cast_value.value_initialized +
                 aggregate_cast_value.default_initialized +
                 global_aggregate_cast_value.first +
                 global_aggregate_cast_value.value_initialized +
                 global_aggregate_cast_value.default_initialized +
                 heap_value->value() +
                 global_class_cast_target.value() +
                 consume(Converting(7)) + cast_value +
                 brace_cast_value + empty_cast_value;
    {
        ClassCastSource class_source(3);
        ClassCastTarget class_cast_value =
            static_cast<ClassCastTarget>(class_source);
        OperatorOnlySource operator_only_source(4);
        ClassCastTarget operator_cast_value =
            static_cast<ClassCastTarget>(operator_only_source);
        result += class_cast_value.value() + operator_cast_value.value();
    }
    if (class_cast_target_destructor_count != 2) {
        delete heap_value;
        return 1;
    }
    delete heap_value;
    return result == 964 ? 0 : 1;
}
