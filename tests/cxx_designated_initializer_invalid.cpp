struct InvalidDesignatedRecord {
    int first;
    int second;
};

InvalidDesignatedRecord out_of_order{.second = 2, .first = 1};
