struct NestedDesignatedRecord {
    int first;
    int second;
};

NestedDesignatedRecord nested{.first.second = 1};
