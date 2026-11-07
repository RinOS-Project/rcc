class MidPriorityRecord {
public:
    explicit MidPriorityRecord(int) {}
};

MidPriorityRecord mid_priority_record __attribute__((init_priority(150))) {5};
