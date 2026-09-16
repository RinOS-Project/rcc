/* Pointer provenance must survive constexpr aggregate return and copy. */

struct PointerHolder {
    int* pointer;
    int value;
};

int constexpr_pointer_aggregate_values[3] = {17, 19, 23};

constexpr PointerHolder make_pointer_holder(int* pointer, int value) {
    return PointerHolder{pointer, value};
}

constexpr PointerHolder returned_holder =
    make_pointer_holder(&constexpr_pointer_aggregate_values[1], 41);
constexpr PointerHolder copied_holder = returned_holder;
constexpr int* extracted_pointer = copied_holder.pointer;

static_assert(extracted_pointer == &constexpr_pointer_aggregate_values[1],
              "constexpr aggregate pointer provenance was lost");
static_assert(*extracted_pointer == 19,
              "constexpr aggregate pointer does not address the target");
static_assert(copied_holder.value == 41,
              "constexpr aggregate copy lost a scalar member");

extern "C" int probe_constexpr_pointer_aggregate(void) {
    return extracted_pointer == &constexpr_pointer_aggregate_values[1] &&
                   *extracted_pointer == 19 && copied_holder.value == 41
               ? 0
               : 1;
}

int main(void) {
    return probe_constexpr_pointer_aggregate();
}
