static int typeof_vla_typedef_capture(void) {
    int bound = 2;
    typedef typeof(int[bound++]) Captured;
    if (bound != 3) return 60 + bound;

    Captured first, second;
    if (bound != 3) return 70 + bound;
    if (sizeof(first) != 2u * sizeof(int)) return 80;
    if (sizeof(second) != 2u * sizeof(int)) return 90;

    bound = 5;
    Captured third;
    if (sizeof(third) != 2u * sizeof(int)) return 100;

    int nested_bound = 2;
    typedef typeof(int[nested_bound++]) Row;
    if (nested_bound != 3) return 110 + nested_bound;
    typedef typeof(Row[nested_bound++]) Matrix;
    if (nested_bound != 4) return 120 + nested_bound;
    Matrix matrix;
    if (sizeof(matrix) != 3u * 2u * sizeof(int)) return 130;
    nested_bound = 8;
    Matrix another_matrix;
    if (sizeof(another_matrix) != 3u * 2u * sizeof(int)) return 140;
    if (nested_bound != 8) return 150;
    return 0;
}

static int typeof_prototype_scope_contexts(void) {
    int typedef_count = 3;
    typedef typeof(int[typedef_count++]) Row;
    void consume_row(Row values);
    if (typedef_count != 4) return 160 + typedef_count;

    int direct_count = 3;
    void consume_direct(typeof(int[direct_count++]) values);
    if (direct_count != 3) return 170 + direct_count;

    int nested_count = 2;
    void consume_nested(
        void (*callback)(typeof(typeof(int[nested_count++])[
            nested_count + 1]) values));
    if (nested_count != 2) return 180 + nested_count;
    return 0;
}

static int typeof_declarator_size_interaction(void) {
    int base_count = 2;
    int object_count = 4;
    typeof(int[base_count++]) values[object_count++];
    if (base_count != 3 || object_count != 5) return 190;
    if (sizeof(values) != 2u * 4u * sizeof(int)) return 200;
    return 0;
}

int main(void) {
    int bound = 3;
    typeof(int[bound++]) first, second;
    if (bound != 4) return 10 + bound;
    if (sizeof(first) != 3u * sizeof(int)) return 20;
    if (sizeof(second) != 3u * sizeof(int)) return 30;
    typeof(int[bound++]) third;
    if (bound != 5) return 40 + bound;
    if (sizeof(third) != 4u * sizeof(int)) return 50;
    int typedef_status = typeof_vla_typedef_capture();
    if (typedef_status != 0) return typedef_status;
    int prototype_status = typeof_prototype_scope_contexts();
    if (prototype_status != 0) return prototype_status;
    return typeof_declarator_size_interaction();
}
