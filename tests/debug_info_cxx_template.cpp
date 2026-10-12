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
