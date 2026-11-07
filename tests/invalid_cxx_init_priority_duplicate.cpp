class InitPriorityDuplicateRecord {};
InitPriorityDuplicateRecord invalid_duplicate
    __attribute__((init_priority(101)))
    __attribute__((init_priority(102))) {};
