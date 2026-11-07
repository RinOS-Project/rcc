class InitPriorityLocalRecord {};

void init_priority_local_scope(void) {
    InitPriorityLocalRecord invalid_local
        __attribute__((init_priority(101))) {};
}
