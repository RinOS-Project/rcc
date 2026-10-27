extern "C" {
int cxx_static_reference_events = 0;
int cxx_static_reference_expected_events = 21;
#ifdef RCC_STATIC_REFERENCE_TLS_TEST
int cxx_static_reference_tls_constructions = 0;
int cxx_static_reference_tls_destructions = 0;
unsigned long long cxx_static_reference_tls_destruction_order = 0;
int cxx_static_reference_tls_base_constructions = 0;
int cxx_static_reference_tls_derived_constructions = 0;
int cxx_static_reference_tls_base_destructions = 0;
int cxx_static_reference_tls_derived_destructions = 0;
int cxx_static_reference_tls_conversions = 0;
int cxx_static_reference_tls_member_constructions = 0;
int cxx_static_reference_tls_member_destructions = 0;
int cxx_static_reference_tls_member_pointer_selections = 0;
int cxx_static_reference_tls_member_pointer_owner_constructions = 0;
int cxx_static_reference_tls_member_pointer_owner_destructions = 0;
int cxx_static_reference_tls_member_pointer_derived_constructions = 0;
int cxx_static_reference_tls_member_pointer_derived_destructions = 0;
int cxx_static_reference_tls_comma_evaluations = 0;
#endif
}

struct StaticReferenceLifetime {
    int value;

    ~StaticReferenceLifetime() {
        cxx_static_reference_events =
                cxx_static_reference_events * 10 + value;
    }
};

const StaticReferenceLifetime& global_static_reference =
        StaticReferenceLifetime{1};

#ifdef RCC_STATIC_REFERENCE_TLS_TEST
struct ThreadLocalReferenceLifetime {
    int value;

    explicit ThreadLocalReferenceLifetime(int initial_value)
        : value(initial_value) {
        ++cxx_static_reference_tls_constructions;
    }

    ~ThreadLocalReferenceLifetime() {
        ++cxx_static_reference_tls_destructions;
        cxx_static_reference_tls_destruction_order =
            cxx_static_reference_tls_destruction_order * 10 + value;
    }
};

struct ThreadLocalReferenceBase {
    int value;

    explicit ThreadLocalReferenceBase(int initial_value)
        : value(initial_value) {
        ++cxx_static_reference_tls_base_constructions;
    }

    ~ThreadLocalReferenceBase() {
        ++cxx_static_reference_tls_base_destructions;
        cxx_static_reference_tls_destruction_order =
            cxx_static_reference_tls_destruction_order * 10 + 1;
    }
};

static const ThreadLocalReferenceBase* thread_local_block_reference(
    int initial_value) {
    static thread_local const ThreadLocalReferenceBase& reference =
        ThreadLocalReferenceBase{initial_value};
    return &reference;
}

struct ThreadLocalReferenceDerived : ThreadLocalReferenceBase {
    explicit ThreadLocalReferenceDerived(int initial_value)
        : ThreadLocalReferenceBase(initial_value) {
        ++cxx_static_reference_tls_derived_constructions;
    }

    ~ThreadLocalReferenceDerived() {
        ++cxx_static_reference_tls_derived_destructions;
        cxx_static_reference_tls_destruction_order =
            cxx_static_reference_tls_destruction_order * 10 + 2;
    }
};

static const ThreadLocalReferenceBase*
thread_local_conditional_block_reference(int initial_value, int choose_first) {
    static thread_local const ThreadLocalReferenceBase& reference =
        choose_first
            ? ThreadLocalReferenceDerived{initial_value}
            : ThreadLocalReferenceDerived{initial_value + 1};
    return &reference;
}

struct ThreadLocalReferenceConversionSource {
    int value;

    operator ThreadLocalReferenceDerived() {
        ++cxx_static_reference_tls_conversions;
        return ThreadLocalReferenceDerived{value + 20};
    }
};

struct ThreadLocalReferenceMemberOwner {
    int value;

    explicit ThreadLocalReferenceMemberOwner(int initial_value)
        : value(initial_value) {
        ++cxx_static_reference_tls_member_constructions;
    }

    ~ThreadLocalReferenceMemberOwner() {
        ++cxx_static_reference_tls_member_destructions;
        cxx_static_reference_tls_destruction_order =
            cxx_static_reference_tls_destruction_order * 10 + 5;
    }
};

struct ThreadLocalSelectedMemberOwner {
    int value;

    explicit ThreadLocalSelectedMemberOwner(int initial_value)
        : value(initial_value) {
        ++cxx_static_reference_tls_member_pointer_owner_constructions;
    }

    ~ThreadLocalSelectedMemberOwner() {
        ++cxx_static_reference_tls_member_pointer_owner_destructions;
    }
};

int ThreadLocalSelectedMemberOwner::*thread_local_selected_member_pointer =
    &ThreadLocalSelectedMemberOwner::value;

int ThreadLocalReferenceMemberOwner::*thread_local_reference_member_pointer =
    &ThreadLocalReferenceMemberOwner::value;

struct ThreadLocalSelectedMemberDerivedOwner
    : ThreadLocalSelectedMemberOwner {
    explicit ThreadLocalSelectedMemberDerivedOwner(int initial_value)
        : ThreadLocalSelectedMemberOwner(initial_value) {
        ++cxx_static_reference_tls_member_pointer_derived_constructions;
    }

    ~ThreadLocalSelectedMemberDerivedOwner() {
        ++cxx_static_reference_tls_member_pointer_derived_destructions;
    }
};

static int ThreadLocalSelectedMemberOwner::*
select_thread_local_reference_member_pointer() {
    ++cxx_static_reference_tls_member_pointer_selections;
    return thread_local_selected_member_pointer;
}

thread_local int cxx_static_reference_thread_value;

thread_local const ThreadLocalReferenceLifetime& thread_local_reference =
    ThreadLocalReferenceLifetime{cxx_static_reference_thread_value};

thread_local const ThreadLocalReferenceLifetime&
    thread_local_comma_reference =
        (cxx_static_reference_tls_comma_evaluations++,
         ThreadLocalReferenceLifetime{
             cxx_static_reference_thread_value + 1});

thread_local const ThreadLocalReferenceBase& thread_local_base_reference =
    static_cast<ThreadLocalReferenceBase&&>(
        ThreadLocalReferenceDerived{cxx_static_reference_thread_value + 10});

thread_local const ThreadLocalReferenceBase&
    thread_local_converted_base_reference =
        ThreadLocalReferenceConversionSource{
            cxx_static_reference_thread_value + 20};

thread_local const ThreadLocalReferenceBase&
    thread_local_conditional_reference =
        (cxx_static_reference_thread_value & 1) != 0
            ? ThreadLocalReferenceDerived{
                  cxx_static_reference_thread_value + 30}
            : ThreadLocalReferenceDerived{
                  cxx_static_reference_thread_value + 40};

thread_local const int& thread_local_member_reference =
    ThreadLocalReferenceMemberOwner{
        cxx_static_reference_thread_value + 60}.*
        thread_local_reference_member_pointer;

thread_local const int& thread_local_selected_member_reference =
    ThreadLocalSelectedMemberDerivedOwner{
        cxx_static_reference_thread_value + 65}.*
        select_thread_local_reference_member_pointer();

extern "C" int cxx_static_reference_tls_worker(int value) {
    cxx_static_reference_thread_value = value;
    const ThreadLocalReferenceLifetime* first = &thread_local_reference;
    const ThreadLocalReferenceLifetime* second = &thread_local_reference;
    if (first != second || first->value != value) return 1;
    const ThreadLocalReferenceLifetime* comma_first =
        &thread_local_comma_reference;
    const ThreadLocalReferenceLifetime* comma_second =
        &thread_local_comma_reference;
    if (comma_first != comma_second || comma_first->value != value + 1) {
        return 8;
    }
    const ThreadLocalReferenceBase* base_first = &thread_local_base_reference;
    const ThreadLocalReferenceBase* base_second = &thread_local_base_reference;
    if (base_first != base_second || base_first->value != value + 10) return 2;
    const ThreadLocalReferenceBase* converted_first =
        &thread_local_converted_base_reference;
    const ThreadLocalReferenceBase* converted_second =
        &thread_local_converted_base_reference;
    if (converted_first != converted_second ||
        converted_first->value != value + 40) return 3;
    const int* member_first = &thread_local_member_reference;
    const int* member_second = &thread_local_member_reference;
    if (member_first != member_second || *member_first != value + 60) return 5;
    const int* selected_member_first =
        &thread_local_selected_member_reference;
    const int* selected_member_second =
        &thread_local_selected_member_reference;
    if (selected_member_first != selected_member_second ||
        *selected_member_first != value + 65) {
        return 10;
    }
    const ThreadLocalReferenceBase& local_converted_reference =
        ThreadLocalReferenceConversionSource{value + 30};
    if (local_converted_reference.value != value + 50) return 4;
    const ThreadLocalReferenceBase* block_first =
        thread_local_block_reference(value + 70);
    /* A different second value verifies per-thread initialization only once. */
    const ThreadLocalReferenceBase* block_second =
        thread_local_block_reference(value + 71);
    if (block_first != block_second || block_first->value != value + 70) {
        return 6;
    }
    const ThreadLocalReferenceBase* conditional_first =
        thread_local_conditional_block_reference(value + 80, 1);
    const ThreadLocalReferenceBase* conditional_second =
        thread_local_conditional_block_reference(value + 81, 0);
    if (conditional_first != conditional_second ||
        conditional_first->value != value + 80) {
        return 7;
    }
    const ThreadLocalReferenceBase* namespace_conditional_first =
        &thread_local_conditional_reference;
    const ThreadLocalReferenceBase* namespace_conditional_second =
        &thread_local_conditional_reference;
    int namespace_conditional_value = value + ((value & 1) ? 30 : 40);
    if (namespace_conditional_first != namespace_conditional_second ||
        namespace_conditional_first->value != namespace_conditional_value) {
        return 9;
    }
    return 0;
}
#endif

const StaticReferenceLifetime& local_static_reference() {
    static const StaticReferenceLifetime& value =
            StaticReferenceLifetime{2};
    return value;
}

static int scalar_reference_initializations = 0;

int make_static_reference_scalar(int value) {
    ++scalar_reference_initializations;
    return value;
}

const int& global_scalar_reference = make_static_reference_scalar(5);

const int& local_scalar_reference() {
    static const int& value = make_static_reference_scalar(6);
    return value;
}

int main() {
    if (global_scalar_reference != 5) return 1;
    const int& first_local_scalar = local_scalar_reference();
    const int& second_local_scalar = local_scalar_reference();
    if (first_local_scalar != 6) return 2;
    if (second_local_scalar != 6) return 3;
    if (scalar_reference_initializations != 2) return 4;
    if (global_static_reference.value != 1) return 5;
    if (local_static_reference().value != 2) return 6;
    if (local_static_reference().value != 2) return 7;
    return cxx_static_reference_events == 0 ? 0 : 8;
}
