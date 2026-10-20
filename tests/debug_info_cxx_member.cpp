typedef unsigned int DebugMemberWord;
using DebugUsingWord = unsigned short;

namespace DebugAliasScope {
using NamespaceWord = unsigned long long;
}

class DebugMemberObject {
private:
    using PrivateWord = unsigned int;
    int secret : 4;
    int secret_value() const { return secret; }
protected:
    using ProtectedWord = unsigned long long;
    int protected_value;
    int protected_read() const { return protected_value; }
public:
    using PublicWord = unsigned short;
    int value;
    int read() const { return value; }
    static int create_value(int seed) { return seed + 1; }
};

struct DebugInheritanceLeft {
    int left;
};

struct DebugInheritanceRight {
    char right;
};

class DebugInheritanceDerived : public DebugInheritanceLeft,
                                private DebugInheritanceRight {
public:
    short own;
};

struct DebugVirtualInheritanceBase {
    int virtual_member;
};

struct DebugVirtualInheritanceOther {
    short other_member;
};

struct DebugVirtualInheritanceDerived : virtual DebugVirtualInheritanceBase,
                                        virtual DebugVirtualInheritanceOther {
    int direct;
};

struct DebugVirtualInheritanceLayer : DebugVirtualInheritanceDerived {
    short layer;
};

int debug_member_object_entry(DebugMemberObject* object) {
    return object->read() + DebugMemberObject::create_value(object->value);
}

DebugMemberWord debug_typedef_entry(DebugMemberWord value) {
    return value;
}

DebugUsingWord debug_using_alias_entry(DebugUsingWord value) {
    return value;
}

int debug_local_alias_entry(int value) {
    using LocalWord = unsigned short;
    LocalWord local_value = static_cast<LocalWord>(value);
    {
        using NestedWord = unsigned long long;
        NestedWord nested_value = static_cast<NestedWord>(local_value);
        value += static_cast<int>(nested_value);
    }
    return value + static_cast<int>(local_value);
}

template<typename T>
T debug_template_local_alias_entry(T value) {
    using TemplateWord = T;
    TemplateWord local_value = value;
    return local_value;
}

int debug_template_local_alias_caller(int value) {
    return static_cast<int>(debug_template_local_alias_entry<unsigned short>(
        static_cast<unsigned short>(value)));
}

int debug_inheritance_entry(DebugInheritanceDerived* object) {
    return object->left + object->own;
}

int debug_virtual_inheritance_entry(DebugVirtualInheritanceLayer* object) {
    return object->virtual_member + object->direct + object->layer;
}

int debug_reference_type_entry(int& lvalue, int&& rvalue) {
    int& local_lvalue = lvalue;
    return local_lvalue + rvalue;
}

extern "C" int debug_cxx_for_initializer_scope(int limit) {
    int outer_value = 0;
    for (int loop_index = 0; loop_index < limit; ++loop_index) {
        outer_value += loop_index;
    }
    return outer_value;
}

static inline int debug_nullptr_parameter_inline(int* value, int* other)
{
    return value == other;
}

extern "C" int debug_nullptr_parameter_entry(int* other)
{
    return debug_nullptr_parameter_inline(nullptr, other);
}

using DebugDataMemberPointer = int DebugMemberObject::*;
using DebugFunctionMemberPointer = int (DebugMemberObject::*)() const;

DebugDataMemberPointer debug_data_member_pointer;
DebugFunctionMemberPointer debug_function_member_pointer;

struct DebugInlineAdjustedBase {
    int value;
};

struct DebugInlineAdjustedPrefix {
    int prefix;
};

struct DebugInlineAdjustedDerived : DebugInlineAdjustedPrefix,
                                    DebugInlineAdjustedBase {
    int suffix;
};

struct DebugInlineAutomaticAddressAggregate {
    int prefix;
    int member;
    int values[3];
};

enum class DebugInlineConstexprEnum : signed char {
    negative = -37,
};

constexpr DebugInlineConstexprEnum debug_inline_constexpr_enum_value =
    DebugInlineConstexprEnum::negative;

extern "C" {
DebugInlineAdjustedDerived debug_inline_adjusted_pointer_source;
DebugInlineAdjustedDerived* debug_inline_adjusted_automatic_escape;
DebugInlineAutomaticAddressAggregate*
    debug_inline_automatic_aggregate_escape;
int** debug_inline_automatic_pointer_slot_escape;
int* debug_inline_automatic_pointer_value_escape;
void* debug_inline_automatic_index_slot_escape;
void* debug_inline_automatic_second_index_slot_escape;
void* debug_inline_automatic_many_index_slot_escape[17];

inline int debug_inline_adjusted_base_parameter_inline(
    DebugInlineAdjustedBase* value) {
    return value != nullptr;
}

int debug_inline_adjusted_base_entry() {
    return debug_inline_adjusted_base_parameter_inline(
        static_cast<DebugInlineAdjustedBase*>(
            &debug_inline_adjusted_pointer_source));
}

int debug_inline_adjusted_base_automatic_entry() {
    DebugInlineAdjustedDerived debug_inline_adjusted_automatic_source;
    int result = debug_inline_adjusted_base_parameter_inline(
        static_cast<DebugInlineAdjustedBase*>(
            &debug_inline_adjusted_automatic_source));
    debug_inline_adjusted_automatic_escape =
        &debug_inline_adjusted_automatic_source;
    return result;
}

inline int debug_inline_automatic_subobject_parameter_inline(int* value) {
    return value != nullptr;
}

int debug_inline_automatic_member_address_entry() {
    DebugInlineAutomaticAddressAggregate
        debug_inline_automatic_member_source;
    int result = debug_inline_automatic_subobject_parameter_inline(
        &debug_inline_automatic_member_source.member);
    debug_inline_automatic_aggregate_escape =
        &debug_inline_automatic_member_source;
    return result;
}

int debug_inline_automatic_array_element_address_entry() {
    DebugInlineAutomaticAddressAggregate
        debug_inline_automatic_array_source;
    int result = debug_inline_automatic_subobject_parameter_inline(
        &debug_inline_automatic_array_source.values[2]);
    debug_inline_automatic_aggregate_escape =
        &debug_inline_automatic_array_source;
    return result;
}

int debug_inline_automatic_pointer_add_entry(int* seed) {
    int* debug_inline_automatic_pointer_source = seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_source;
    int result = debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_source + 2);
    debug_inline_automatic_pointer_value_escape =
        debug_inline_automatic_pointer_source;
    return result;
}

int debug_inline_automatic_pointer_sub_entry(int* seed) {
    int* debug_inline_automatic_pointer_sub_source = seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_sub_source;
    int result = debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_sub_source - 1);
    debug_inline_automatic_pointer_value_escape =
        debug_inline_automatic_pointer_sub_source;
    return result;
}

int debug_inline_automatic_pointer_chain_entry(int* seed) {
    int* debug_inline_automatic_pointer_chain_source = seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_chain_source;
    int result = debug_inline_automatic_subobject_parameter_inline(
        (debug_inline_automatic_pointer_chain_source + 2) - 1);
    debug_inline_automatic_pointer_value_escape =
        debug_inline_automatic_pointer_chain_source;
    return result;
}

int debug_inline_automatic_pointer_index_entry(int* seed, int index_seed) {
    int* debug_inline_automatic_pointer_index_source = seed;
    int debug_inline_automatic_index_source = index_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_index_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_index_source;
    int result = debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_index_source +
        debug_inline_automatic_index_source);
    debug_inline_automatic_pointer_value_escape =
        debug_inline_automatic_pointer_index_source;
    return result;
}

int debug_inline_automatic_pointer_index_sub_entry(
    int* seed, unsigned int index_seed) {
    int* debug_inline_automatic_pointer_index_sub_source = seed;
    unsigned int debug_inline_automatic_index_sub_source = index_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_index_sub_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_index_sub_source;
    int result = debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_index_sub_source -
        debug_inline_automatic_index_sub_source);
    debug_inline_automatic_pointer_value_escape =
        debug_inline_automatic_pointer_index_sub_source;
    return result;
}

int debug_inline_automatic_pointer_narrow_index_entry(
    int* seed, signed char index_seed) {
    int* debug_inline_automatic_pointer_narrow_index_source = seed;
    signed char debug_inline_automatic_narrow_index_source = index_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_narrow_index_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_narrow_index_source;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_narrow_index_source +
        debug_inline_automatic_narrow_index_source);
}

int debug_inline_automatic_pointer_compound_index_entry(
    int* seed, signed char first_index_seed,
    unsigned int second_index_seed) {
    int* debug_inline_automatic_pointer_compound_source = seed;
    signed char debug_inline_automatic_compound_signed_index =
        first_index_seed;
    unsigned int debug_inline_automatic_compound_unsigned_index =
        second_index_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_compound_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_compound_signed_index;
    debug_inline_automatic_second_index_slot_escape =
        &debug_inline_automatic_compound_unsigned_index;
    return debug_inline_automatic_subobject_parameter_inline(
        (debug_inline_automatic_pointer_compound_source +
         debug_inline_automatic_compound_signed_index) -
        debug_inline_automatic_compound_unsigned_index);
}

int debug_inline_automatic_pointer_parameter_index_entry(
    int* seed, int debug_inline_automatic_parameter_index_source) {
    int* debug_inline_automatic_pointer_parameter_source = seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_parameter_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_parameter_index_source;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_parameter_source +
        debug_inline_automatic_parameter_index_source);
}

int debug_inline_automatic_pointer_computed_index_entry(
    int* seed, signed char first_index_seed, int second_index_seed) {
    int* debug_inline_automatic_pointer_computed_source = seed;
    signed char debug_inline_automatic_computed_signed_index =
        first_index_seed;
    int debug_inline_automatic_computed_int_index = second_index_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_computed_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_computed_signed_index;
    debug_inline_automatic_second_index_slot_escape =
        &debug_inline_automatic_computed_int_index;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_computed_source +
        (debug_inline_automatic_computed_signed_index +
         debug_inline_automatic_computed_int_index));
}

int debug_inline_automatic_pointer_unsigned_computed_index_entry(
    int* seed, unsigned long first_index_seed,
    unsigned long second_index_seed) {
    int* debug_inline_automatic_pointer_unsigned_computed_source = seed;
    unsigned long debug_inline_automatic_unsigned_first_index =
        first_index_seed;
    unsigned long debug_inline_automatic_unsigned_second_index =
        second_index_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_unsigned_computed_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_unsigned_first_index;
    debug_inline_automatic_second_index_slot_escape =
        &debug_inline_automatic_unsigned_second_index;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_unsigned_computed_source +
        (debug_inline_automatic_unsigned_first_index -
         debug_inline_automatic_unsigned_second_index));
}

int debug_inline_automatic_pointer_narrow_unsigned_computed_index_entry(
    int* seed, unsigned int first_index_seed,
    unsigned int second_index_seed) {
    int* debug_inline_automatic_pointer_narrow_unsigned_computed_source =
        seed;
    unsigned int debug_inline_automatic_narrow_unsigned_first_index =
        first_index_seed;
    unsigned int debug_inline_automatic_narrow_unsigned_second_index =
        second_index_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_narrow_unsigned_computed_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_narrow_unsigned_first_index;
    debug_inline_automatic_second_index_slot_escape =
        &debug_inline_automatic_narrow_unsigned_second_index;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_narrow_unsigned_computed_source +
        (debug_inline_automatic_narrow_unsigned_first_index -
         debug_inline_automatic_narrow_unsigned_second_index));
}

int debug_inline_automatic_pointer_narrow_unsigned_constant_index_entry(
    int* seed, unsigned int index_seed) {
    int* debug_inline_automatic_pointer_narrow_unsigned_constant_source =
        seed;
    unsigned int debug_inline_automatic_narrow_unsigned_constant_index =
        index_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_narrow_unsigned_constant_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_narrow_unsigned_constant_index;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_narrow_unsigned_constant_source +
        static_cast<unsigned long>(
            debug_inline_automatic_narrow_unsigned_constant_index + 1u));
}

int debug_inline_automatic_pointer_signed_to_unsigned_index_entry(
    int* seed, int index_seed) {
    int* debug_inline_automatic_pointer_signed_to_unsigned_source = seed;
    int debug_inline_automatic_signed_to_unsigned_index = index_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_signed_to_unsigned_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_signed_to_unsigned_index;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_signed_to_unsigned_source +
        static_cast<unsigned int>(
            debug_inline_automatic_signed_to_unsigned_index));
}

int debug_inline_automatic_pointer_signed_narrow_index_entry(
    int* seed, int index_seed) {
    int* debug_inline_automatic_pointer_signed_narrow_source = seed;
    int debug_inline_automatic_signed_narrow_index = index_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_signed_narrow_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_signed_narrow_index;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_signed_narrow_source +
        static_cast<short>(debug_inline_automatic_signed_narrow_index));
}

int debug_inline_automatic_pointer_signed_narrow_widen_index_entry(
    int* seed, int index_seed) {
    int* debug_inline_automatic_pointer_signed_narrow_widen_source = seed;
    int debug_inline_automatic_signed_narrow_widen_index = index_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_signed_narrow_widen_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_signed_narrow_widen_index;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_signed_narrow_widen_source +
        static_cast<int>(static_cast<short>(
            debug_inline_automatic_signed_narrow_widen_index)));
}

int debug_inline_automatic_pointer_signed_narrow_unsigned_index_entry(
    int* seed, int index_seed) {
    int* debug_inline_automatic_pointer_signed_narrow_unsigned_source = seed;
    int debug_inline_automatic_signed_narrow_unsigned_index = index_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_signed_narrow_unsigned_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_signed_narrow_unsigned_index;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_signed_narrow_unsigned_source +
        static_cast<unsigned int>(static_cast<short>(
            debug_inline_automatic_signed_narrow_unsigned_index)));
}

int debug_inline_automatic_pointer_unsigned_narrow_signed_index_entry(
    int* seed, int index_seed) {
    int* debug_inline_automatic_pointer_unsigned_narrow_signed_source = seed;
    int debug_inline_automatic_unsigned_narrow_signed_index = index_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_unsigned_narrow_signed_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_unsigned_narrow_signed_index;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_unsigned_narrow_signed_source +
        static_cast<int>(static_cast<unsigned short>(
            debug_inline_automatic_unsigned_narrow_signed_index)));
}

int debug_inline_automatic_pointer_multistage_signed_widen_index_entry(
    int* seed, int index_seed) {
    int* debug_inline_automatic_pointer_multistage_signed_widen_source = seed;
    int debug_inline_automatic_multistage_signed_widen_index = index_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_multistage_signed_widen_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_multistage_signed_widen_index;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_multistage_signed_widen_source +
        static_cast<long>(static_cast<int>(static_cast<short>(
            debug_inline_automatic_multistage_signed_widen_index))));
}

int debug_inline_automatic_pointer_multistage_narrow_index_entry(
    int* seed, int index_seed) {
    int* debug_inline_automatic_pointer_multistage_narrow_source = seed;
    int debug_inline_automatic_multistage_narrow_index = index_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_multistage_narrow_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_multistage_narrow_index;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_multistage_narrow_source +
        static_cast<int>(static_cast<short>(static_cast<signed char>(
            static_cast<short>(
                debug_inline_automatic_multistage_narrow_index)))));
}

int debug_inline_automatic_pointer_unsigned_narrow_index_entry(
    int* seed, int index_seed) {
    int* debug_inline_automatic_pointer_unsigned_narrow_source = seed;
    int debug_inline_automatic_unsigned_narrow_index = index_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_unsigned_narrow_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_unsigned_narrow_index;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_unsigned_narrow_source +
        static_cast<unsigned short>(
            debug_inline_automatic_unsigned_narrow_index));
}

int debug_inline_automatic_pointer_mixed_width_index_entry(
    int* seed, int signed_index_seed,
    unsigned long long unsigned_index_seed) {
    int* debug_inline_automatic_pointer_mixed_width_source = seed;
    int debug_inline_automatic_mixed_width_signed_index = signed_index_seed;
    unsigned long long debug_inline_automatic_mixed_width_unsigned_index =
        unsigned_index_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_mixed_width_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_mixed_width_signed_index;
    debug_inline_automatic_second_index_slot_escape =
        &debug_inline_automatic_mixed_width_unsigned_index;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_mixed_width_source +
        (static_cast<unsigned long long>(
             debug_inline_automatic_mixed_width_signed_index) +
         debug_inline_automatic_mixed_width_unsigned_index));
}

int debug_inline_automatic_pointer_signed_to_narrow_unsigned_index_entry(
    int* seed, short index_seed) {
    int* debug_inline_automatic_pointer_signed_to_narrow_unsigned_source =
        seed;
    short debug_inline_automatic_signed_to_narrow_unsigned_index =
        index_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_signed_to_narrow_unsigned_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_signed_to_narrow_unsigned_index;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_signed_to_narrow_unsigned_source +
        static_cast<unsigned int>(
            debug_inline_automatic_signed_to_narrow_unsigned_index));
}

int debug_inline_automatic_pointer_large_unsigned_constant_index_entry(
    int* seed) {
    int* debug_inline_automatic_pointer_large_unsigned_constant_source = seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_large_unsigned_constant_source;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_large_unsigned_constant_source +
        0x8000000000000000ULL);
}

int debug_inline_automatic_pointer_many_unsigned_index_entry(
    int* seed, unsigned int seed0, unsigned int seed1,
    unsigned int seed2, unsigned int seed3, unsigned int seed4,
    unsigned int seed5, unsigned int seed6, unsigned int seed7,
    unsigned int seed8, unsigned int seed9, unsigned int seed10,
    unsigned int seed11, unsigned int seed12, unsigned int seed13,
    unsigned int seed14, unsigned int seed15, unsigned int seed16) {
    int* debug_inline_automatic_pointer_many_unsigned_source = seed;
    unsigned int debug_inline_automatic_many_unsigned_index_0 = seed0;
    unsigned int debug_inline_automatic_many_unsigned_index_1 = seed1;
    unsigned int debug_inline_automatic_many_unsigned_index_2 = seed2;
    unsigned int debug_inline_automatic_many_unsigned_index_3 = seed3;
    unsigned int debug_inline_automatic_many_unsigned_index_4 = seed4;
    unsigned int debug_inline_automatic_many_unsigned_index_5 = seed5;
    unsigned int debug_inline_automatic_many_unsigned_index_6 = seed6;
    unsigned int debug_inline_automatic_many_unsigned_index_7 = seed7;
    unsigned int debug_inline_automatic_many_unsigned_index_8 = seed8;
    unsigned int debug_inline_automatic_many_unsigned_index_9 = seed9;
    unsigned int debug_inline_automatic_many_unsigned_index_10 = seed10;
    unsigned int debug_inline_automatic_many_unsigned_index_11 = seed11;
    unsigned int debug_inline_automatic_many_unsigned_index_12 = seed12;
    unsigned int debug_inline_automatic_many_unsigned_index_13 = seed13;
    unsigned int debug_inline_automatic_many_unsigned_index_14 = seed14;
    unsigned int debug_inline_automatic_many_unsigned_index_15 = seed15;
    unsigned int debug_inline_automatic_many_unsigned_index_16 = seed16;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_many_unsigned_source;
    debug_inline_automatic_many_index_slot_escape[0] =
        &debug_inline_automatic_many_unsigned_index_0;
    debug_inline_automatic_many_index_slot_escape[1] =
        &debug_inline_automatic_many_unsigned_index_1;
    debug_inline_automatic_many_index_slot_escape[2] =
        &debug_inline_automatic_many_unsigned_index_2;
    debug_inline_automatic_many_index_slot_escape[3] =
        &debug_inline_automatic_many_unsigned_index_3;
    debug_inline_automatic_many_index_slot_escape[4] =
        &debug_inline_automatic_many_unsigned_index_4;
    debug_inline_automatic_many_index_slot_escape[5] =
        &debug_inline_automatic_many_unsigned_index_5;
    debug_inline_automatic_many_index_slot_escape[6] =
        &debug_inline_automatic_many_unsigned_index_6;
    debug_inline_automatic_many_index_slot_escape[7] =
        &debug_inline_automatic_many_unsigned_index_7;
    debug_inline_automatic_many_index_slot_escape[8] =
        &debug_inline_automatic_many_unsigned_index_8;
    debug_inline_automatic_many_index_slot_escape[9] =
        &debug_inline_automatic_many_unsigned_index_9;
    debug_inline_automatic_many_index_slot_escape[10] =
        &debug_inline_automatic_many_unsigned_index_10;
    debug_inline_automatic_many_index_slot_escape[11] =
        &debug_inline_automatic_many_unsigned_index_11;
    debug_inline_automatic_many_index_slot_escape[12] =
        &debug_inline_automatic_many_unsigned_index_12;
    debug_inline_automatic_many_index_slot_escape[13] =
        &debug_inline_automatic_many_unsigned_index_13;
    debug_inline_automatic_many_index_slot_escape[14] =
        &debug_inline_automatic_many_unsigned_index_14;
    debug_inline_automatic_many_index_slot_escape[15] =
        &debug_inline_automatic_many_unsigned_index_15;
    debug_inline_automatic_many_index_slot_escape[16] =
        &debug_inline_automatic_many_unsigned_index_16;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_many_unsigned_source +
        (debug_inline_automatic_many_unsigned_index_0 +
         debug_inline_automatic_many_unsigned_index_1 +
         debug_inline_automatic_many_unsigned_index_2 +
         debug_inline_automatic_many_unsigned_index_3 +
         debug_inline_automatic_many_unsigned_index_4 +
         debug_inline_automatic_many_unsigned_index_5 +
         debug_inline_automatic_many_unsigned_index_6 +
         debug_inline_automatic_many_unsigned_index_7 +
         debug_inline_automatic_many_unsigned_index_8 +
         debug_inline_automatic_many_unsigned_index_9 +
         debug_inline_automatic_many_unsigned_index_10 +
         debug_inline_automatic_many_unsigned_index_11 +
         debug_inline_automatic_many_unsigned_index_12 +
         debug_inline_automatic_many_unsigned_index_13 +
         debug_inline_automatic_many_unsigned_index_14 +
         debug_inline_automatic_many_unsigned_index_15 +
         debug_inline_automatic_many_unsigned_index_16));
}

int debug_inline_automatic_pointer_scaled_unsigned_index_entry(
    int* seed, unsigned int scaled_seed, unsigned int add_seed) {
    int* debug_inline_automatic_pointer_scaled_unsigned_source = seed;
    unsigned int debug_inline_automatic_scaled_unsigned_index = scaled_seed;
    unsigned int debug_inline_automatic_add_unsigned_index = add_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_scaled_unsigned_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_scaled_unsigned_index;
    debug_inline_automatic_second_index_slot_escape =
        &debug_inline_automatic_add_unsigned_index;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_scaled_unsigned_source +
        (debug_inline_automatic_scaled_unsigned_index * 3u +
         debug_inline_automatic_add_unsigned_index));
}

int debug_inline_automatic_pointer_scaled_signed_index_entry(
    int* seed, int scaled_seed, int add_seed) {
    int* debug_inline_automatic_pointer_scaled_signed_source = seed;
    int debug_inline_automatic_scaled_signed_index = scaled_seed;
    int debug_inline_automatic_add_signed_index = add_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_scaled_signed_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_scaled_signed_index;
    debug_inline_automatic_second_index_slot_escape =
        &debug_inline_automatic_add_signed_index;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_scaled_signed_source +
        (debug_inline_automatic_scaled_signed_index * 3 +
         debug_inline_automatic_add_signed_index));
}

int debug_inline_automatic_pointer_neg_scaled_signed_index_entry(
    int* seed, int scaled_seed, int add_seed) {
    int* debug_inline_automatic_pointer_neg_scaled_signed_source = seed;
    int debug_inline_automatic_neg_scaled_signed_index = scaled_seed;
    int debug_inline_automatic_neg_add_signed_index = add_seed;
    debug_inline_automatic_pointer_slot_escape =
        &debug_inline_automatic_pointer_neg_scaled_signed_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_neg_scaled_signed_index;
    debug_inline_automatic_second_index_slot_escape =
        &debug_inline_automatic_neg_add_signed_index;
    return debug_inline_automatic_subobject_parameter_inline(
        debug_inline_automatic_pointer_neg_scaled_signed_source +
        (debug_inline_automatic_neg_scaled_signed_index * -3 +
         debug_inline_automatic_neg_add_signed_index));
}

int debug_inline_automatic_array_index_entry(int index_seed) {
    DebugInlineAutomaticAddressAggregate
        debug_inline_automatic_array_index_source;
    int debug_inline_automatic_array_index = index_seed;
    debug_inline_automatic_aggregate_escape =
        &debug_inline_automatic_array_index_source;
    debug_inline_automatic_index_slot_escape =
        &debug_inline_automatic_array_index;
    return debug_inline_automatic_subobject_parameter_inline(
        &debug_inline_automatic_array_index_source.values[
            debug_inline_automatic_array_index]);
}

constexpr int debug_inline_constexpr_seed = 20;

constexpr int debug_inline_constexpr_add_four(int value) {
    return value + 4;
}

static inline int debug_inline_constexpr_identifier_parameter(int value) {
    return value;
}

static inline int debug_inline_constexpr_call_parameter(int value) {
    return value;
}

static inline int debug_inline_constexpr_enum_parameter(
    DebugInlineConstexprEnum value) {
    return static_cast<int>(value);
}

int debug_inline_constexpr_identifier_entry() {
    return debug_inline_constexpr_identifier_parameter(
        debug_inline_constexpr_seed * 2 + 1);
}

int debug_inline_constexpr_call_entry() {
    return debug_inline_constexpr_call_parameter(
        debug_inline_constexpr_add_four(19));
}

int debug_inline_constexpr_enum_entry() {
    return debug_inline_constexpr_enum_parameter(
        debug_inline_constexpr_enum_value);
}
}
