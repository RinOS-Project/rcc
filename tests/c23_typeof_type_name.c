static int add_two(double value) {
    return (int)value + 2;
}

static int side_effect_count;

static int side_effect_result(void) {
    ++side_effect_count;
    return 21;
}

static int typeof_vla_operand_is_evaluated(void) {
    int width = 3;
    int rows[6][width];
    int (*cursor)[width] = rows;
    if (sizeof(typeof(cursor++)) != sizeof(cursor) || cursor != rows + 1) {
        return 2;
    }
    typeof(cursor++) copied_cursor = cursor;
    if (cursor != rows + 2 || copied_cursor != rows + 2) return 3;
    copied_cursor++;
    if (copied_cursor != rows + 3) return 4;
    if (sizeof(typeof(int[width++])) != 3u * sizeof(int) || width != 4) {
        return 5;
    }
    if (sizeof(typeof(*cursor++)) != 3u * sizeof(int) ||
        cursor != rows + 3) {
        return 6;
    }
    if (sizeof(typeof(*(cursor + 0))) != 3u * sizeof(int) ||
        cursor != rows + 3) {
        return 7;
    }
    typeof(typeof(*cursor++)) row;
    if (cursor != rows + 4) return 8;
    if (sizeof(row) != 3u * sizeof(int)) return 9;
    return 0;
}

static int typeof_typedef_and_generic(void) {
    int source = 8;
    typedef typeof(source) SourceType;
    SourceType copy = 9;
    return _Generic(source, typeof(source): copy, default: 0);
}

int main(void) {
    typeof(int[3]) array = { 1, 2, 3 };
    typeof(int (*)(double)) callback = add_two;
    typeof(const int) preserved_const = 4;
    typeof_unqual(const int) mutable_scalar = 5;
    typeof_unqual(const int[3]) mutable_array = { 6, 7, 8 };
    typeof_unqual(_Atomic(int)) non_atomic = 9;
    typeof_unqual(const int * const) mutable_pointer = &preserved_const;
    typeof(typeof(int)) nested = 10;
    int source = 14;
    int vla_typeof_status = typeof_vla_operand_is_evaluated();
    typeof(source) expression_type = source;
    typeof(source + 2) arithmetic_type = source + 2;
    typeof(source++) unevaluated_side_effect_type = source;
    typeof(side_effect_result()) unevaluated_call_type = 22;
    typeof(&source) pointer_type = &source;
    typeof(*pointer_type) dereferenced_type = 15;
    typeof(add_two) *function_designator_type = &add_two;
    const int qualified_source = 16;
    typeof_unqual(qualified_source) unqualified_expression = 0;
    typeof(array) array_expression_type = { 3, 2, 1 };
    typeof("rcc") string_array_type = "c23";

    mutable_scalar = 11;
    mutable_array[0] = 12;
    mutable_pointer = &mutable_scalar;
    non_atomic = 13;
    unqualified_expression = 17;

    if (vla_typeof_status != 0 || typeof_typedef_and_generic() != 9) {
        return 7;
    }
    if (sizeof(array) != 3u * sizeof(int) ||
        sizeof(callback) != sizeof(void*) ||
        callback(3.0) != 5 || preserved_const != 4 ||
        mutable_scalar != 11 || mutable_array[0] != 12 ||
        *mutable_pointer != 11 || non_atomic != 13 || nested != 10 ||
        expression_type != 14 || arithmetic_type != 16 ||
        unevaluated_side_effect_type != 14 || source != 14 ||
        unevaluated_call_type != 22 || side_effect_count != 0 ||
        *pointer_type != 14 || dereferenced_type != 15 ||
        function_designator_type(4.0) != 6 ||
        unqualified_expression != 17 ||
        array_expression_type[0] != 3 ||
        sizeof(string_array_type) != 4u || string_array_type[0] != 'c') {
        return 1;
    }
    return 0;
}
