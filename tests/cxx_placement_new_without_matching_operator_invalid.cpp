int* allocate_without_matching_placement_operator() {
    return new (7) int;
}
