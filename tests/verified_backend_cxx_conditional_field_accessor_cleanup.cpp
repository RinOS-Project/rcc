static int field_accessor_cleanup_order;
struct FieldAccessorValue;
static const FieldAccessorValue* field_accessor_result_address;

struct FieldAccessorValue {
    int value;
    int cleanup_id;
    int* cleanup_order;

    ~FieldAccessorValue() noexcept {
        if (this == field_accessor_result_address) {
            *cleanup_order = *cleanup_order * 10 + 2;
            field_accessor_result_address = nullptr;
        } else {
            *cleanup_order = *cleanup_order * 10 + cleanup_id;
        }
    }
};

struct FieldAccessorSource {
    FieldAccessorValue value;

    const FieldAccessorValue& get() const noexcept {
        return value;
    }
};

int consume_field_accessor_value(const FieldAccessorValue& value) noexcept {
    field_accessor_result_address = &value;
    return value.value;
}

static int verify_field_accessor_arm(bool choose_field) {
    field_accessor_cleanup_order = 0;
    field_accessor_result_address = nullptr;
    {
        FieldAccessorSource source{
            FieldAccessorValue{41, 1, &field_accessor_cleanup_order}};
        int result = consume_field_accessor_value(
            choose_field
                ? source.get()
                : FieldAccessorValue{82, 3, &field_accessor_cleanup_order});
        if (result != (choose_field ? 41 : 82) ||
            field_accessor_cleanup_order != 2) {
            return 1;
        }
    }
    return field_accessor_cleanup_order == 21 ? 0 : 2;
}

int main() {
    int result = verify_field_accessor_arm(true);
    if (result) return result;
    return verify_field_accessor_arm(false);
}
