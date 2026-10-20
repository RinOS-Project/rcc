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
