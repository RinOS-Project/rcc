class SamePriorityRecord {
public:
    explicit SamePriorityRecord(int) {}
};

SamePriorityRecord same_priority_record __attribute__((init_priority(101))) {6};
