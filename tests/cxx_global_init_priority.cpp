/* Verify C++ global initialization priority and same-priority source order. */

int cxx_global_init_priority_sequence = 0;

class PriorityRecord {
public:
    explicit PriorityRecord(int digit) {
        cxx_global_init_priority_sequence =
            cxx_global_init_priority_sequence * 10 + digit;
    }
};

PriorityRecord priority_two_hundred __attribute__((init_priority(200))) {2};
PriorityRecord priority_one_hundred_one_first
    __attribute__((init_priority(101))) {1};
PriorityRecord priority_one_hundred_one_second
    __attribute__((init_priority(101))) {3};
PriorityRecord priority_default{4};

extern "C" int rcc_global_initializers_main(void) {
    return cxx_global_init_priority_sequence == 1324 ? 0 : 1;
}

int main(void) {
    return rcc_global_initializers_main();
}
