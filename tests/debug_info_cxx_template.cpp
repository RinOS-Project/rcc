struct DebugTemplatePayload {
    unsigned short value;
};

template <typename Element, int Count, int Bias, unsigned int Mask>
struct DebugTemplateBox {
    Element items[Count];
};

DebugTemplateBox<DebugTemplatePayload, 3, -7, 0xF0000000U> debug_template_box;

template <typename Head, typename... Tail>
struct DebugTemplatePack {
    Head head;
};

DebugTemplatePack<int> debug_template_empty_pack;
DebugTemplatePack<int, short, long> debug_template_multiple_pack;

template <typename Head, int... Values>
struct DebugTemplateSignedValuePack {
    Head head;
};

DebugTemplateSignedValuePack<int> debug_template_empty_value_pack;
DebugTemplateSignedValuePack<int, -2, 7> debug_template_signed_value_pack;

template <typename Head, unsigned int... Values>
struct DebugTemplateUnsignedValuePack {
    Head head;
};

DebugTemplateUnsignedValuePack<int, 0xF0000000U>
    debug_template_unsigned_value_pack;

template <int... Values>
struct DebugTemplateEmptyNonTypePack {};

DebugTemplateEmptyNonTypePack<> debug_template_zero_parameter_pack;

template <typename Value, int Bias>
int debug_template_function(Value value) {
    return (int)value + Bias;
}

int debug_template_entry() {
    return debug_template_function<unsigned short, -7>(
        debug_template_box.items[0].value);
}

template <typename Head, typename... Tail>
int debug_template_function_type_pack(Head head, Tail... values) {
    return static_cast<int>(head) + static_cast<int>(sizeof...(Tail));
}

int debug_template_function_type_pack_empty_entry() {
    return debug_template_function_type_pack<int>(1);
}

int debug_template_function_type_pack_multiple_entry() {
    return debug_template_function_type_pack<int, short, long>(1, 2, 3);
}

template <int Seed, int... Values>
int debug_template_function_signed_pack() {
    return Seed + static_cast<int>(sizeof...(Values));
}

int debug_template_function_signed_pack_empty_entry() {
    return debug_template_function_signed_pack<3>();
}

int debug_template_function_signed_pack_multiple_entry() {
    return debug_template_function_signed_pack<3, -2, 7>();
}

template <unsigned int... Values>
int debug_template_function_unsigned_pack() {
    return static_cast<int>(sizeof...(Values));
}

int debug_template_function_unsigned_pack_empty_entry() {
    return debug_template_function_unsigned_pack<>();
}

int debug_template_function_unsigned_pack_entry() {
    return debug_template_function_unsigned_pack<0xF0000000U>();
}
