struct AliasHolder {
    int value;
};

template<typename T> using Identity = T;
template<typename T> using Pointer = T*;

template<typename T>
struct Box {
    T value;
};

template<typename T> using BoxAlias = Box<T>;
template<int N> using IntArray = int[N];

template<typename... Types>
struct AliasPackHolder {
    int marker;
    unsigned char elements[sizeof...(Types) + 1];
};

template<typename... Types>
using AliasPack = AliasPackHolder<Types...>;

template<typename... Types>
using AliasPackForward = AliasPack<Types...>;

template<typename... Types>
using AliasPackForwardAgain = AliasPackForward<Types...>;

template<typename... Types>
using AliasPackForwardAround = AliasPack<char, Types..., long>;

AliasPack<> empty_alias_pack_value = {67, {0}};
AliasPack<int, short> multi_alias_pack_value = {71, {0}};
AliasPackForward<> forwarded_empty_alias_pack_value = {73, {0}};
AliasPackForward<int, short> forwarded_multi_alias_pack_value = {79, {0}};
AliasPackForwardAgain<int, short, char, long, unsigned char>
    forwarded_chain_alias_pack_value = {83, {0}};
AliasPackForwardAround<short, int> forwarded_around_alias_pack_value =
    {89, {0}};

int alias_pack_value(AliasPack<int, short>* value) {
    return value ? value->marker : 0;
}

int alias_pack_around_value(
    AliasPackForwardAround<short, int>* value) {
    return value ? value->marker : 0;
}

int alias_pack_forward_size(
    AliasPackForwardAgain<int, short, char, long, unsigned char>* value) {
    return value ? sizeof(*value) : 0;
}

int alias_pack_around_size(
    AliasPackForwardAround<short, int>* value) {
    return value ? sizeof(*value) : 0;
}

template<int... Values>
struct AliasValuePackHolder {
    int marker;
    unsigned char elements[sizeof...(Values) + 1];
};

template<int... Values>
using AliasValuePack = AliasValuePackHolder<Values...>;

template<int... Values>
using AliasValuePackForward = AliasValuePack<Values...>;

template<int... Values>
using AliasValuePackForwardAgain = AliasValuePackForward<Values...>;

template<int... Values>
using AliasValuePackIncrement =
    AliasValuePackHolder<(Values + 1)...>;

template<int... Values>
using AliasValuePackIncrementForward = AliasValuePackIncrement<Values...>;

template<int... Values>
struct AliasValuePackWrapper {
    AliasValuePackForwardAgain<Values...> nested;
};

AliasValuePack<> empty_alias_value_pack_value = {97, {0}};
AliasValuePack<1, 2, 3, 4, 5> multi_alias_value_pack_value = {101, {0}};
AliasValuePackForward<> empty_forwarded_alias_value_pack_value = {103, {0}};
AliasValuePackForwardAgain<1, 2, 3, 4, 5>
    multi_forwarded_alias_value_pack_value = {107, {0}};
AliasValuePackIncrementForward<2, 3>
    incremented_forwarded_alias_value_pack_value = {109, {0}};
AliasValuePackWrapper<1, 2, 3, 4, 5> wrapped_forwarded_alias_value_pack_value;

int alias_value_pack_size(AliasValuePack<1, 2, 3, 4, 5>* value) {
    return value ? sizeof(*value) : 0;
}

int incremented_alias_value_pack_size(
    AliasValuePackHolder<3, 4>* value) {
    return value ? sizeof(*value) : 0;
}

int wrapped_alias_value_pack_size(
    AliasValuePackWrapper<1, 2, 3, 4, 5>* value) {
    return value ? sizeof(*value) : 0;
}

template<int... Values>
int alias_value_pack_function_size() {
    AliasValuePackWrapper<Values...> value;
    return sizeof(value);
}

enum AliasValuePackEnum {
    AliasValuePackFirst = -1,
    AliasValuePackSecond = 4
};

template<AliasValuePackEnum... Values>
struct AliasEnumValuePackHolder {
    int marker;
    unsigned char elements[sizeof...(Values) + 1];
};

template<AliasValuePackEnum... Values>
using AliasEnumValuePack = AliasEnumValuePackHolder<Values...>;

template<AliasValuePackEnum... Values>
using AliasEnumValuePackForward = AliasEnumValuePack<Values...>;

AliasEnumValuePack<AliasValuePackFirst, AliasValuePackSecond>
    enum_alias_value_pack_value = {113, {0}};
AliasEnumValuePackForward<AliasValuePackFirst, AliasValuePackSecond>
    enum_forwarded_alias_value_pack_value = {127, {0}};

int enum_alias_value_pack_size(
    AliasEnumValuePack<AliasValuePackFirst, AliasValuePackSecond>* value) {
    return value ? sizeof(*value) : 0;
}

struct ClassAliasTemplateOwner {
    template<typename T> using Identity = T;
    template<int N> using Array = int[N];
    using Local = Identity<int>;

protected:
    template<typename T> using ProtectedIdentity = T;

private:
    template<typename T> using PrivateIdentity = T;
};

struct ClassAliasTemplateDerived : ClassAliasTemplateOwner {
    using ProtectedValue =
        ClassAliasTemplateOwner::template ProtectedIdentity<int>;
};

namespace ClassAliasTemplateNamespace {
struct Owner {
    template<typename T> using Identity = T;
};
}

ClassAliasTemplateOwner::template Identity<long> class_alias_value = 47;
ClassAliasTemplateOwner::Identity<Box<int>> nested_class_alias_value =
    {61};
ClassAliasTemplateOwner::Identity<long> alias_without_template_keyword = 59;
typename ClassAliasTemplateNamespace::Owner::template Identity<int>
    namespace_class_alias_value = 53;
ClassAliasTemplateOwner::template Array<3> class_alias_array = {2, 3, 5};
ClassAliasTemplateOwner::Local class_alias_local_value = 7;
ClassAliasTemplateDerived::ProtectedValue protected_alias_value = 11;

int alias_identity(Identity<int> value) {
    return value + 2;
}

int alias_pointer(Pointer<int> value) {
    return *value;
}

int alias_class(BoxAlias<int>* value) {
    return value->value;
}

int main() {
    BoxAlias<int> box;
    box.value = 40;
    IntArray<3> values = {1, 2, 3};
    int value = 5;
    return alias_identity(3) == 5 &&
                   alias_pointer(&value) == 5 &&
                   alias_class(&box) == 40 &&
                   empty_alias_pack_value.marker == 67 &&
                   alias_pack_value(&multi_alias_pack_value) == 71 &&
                   forwarded_empty_alias_pack_value.marker == 73 &&
                   forwarded_multi_alias_pack_value.marker == 79 &&
                   forwarded_chain_alias_pack_value.marker == 83 &&
                   alias_pack_around_value(
                       &forwarded_around_alias_pack_value) == 89 &&
                   alias_pack_forward_size(
                       &forwarded_chain_alias_pack_value) == 12 &&
                   alias_pack_around_size(
                       &forwarded_around_alias_pack_value) == 12 &&
                   empty_alias_value_pack_value.marker == 97 &&
                   multi_alias_value_pack_value.marker == 101 &&
                   alias_value_pack_size(
                       &multi_alias_value_pack_value) == 12 &&
                   empty_forwarded_alias_value_pack_value.marker == 103 &&
                   multi_forwarded_alias_value_pack_value.marker == 107 &&
                   alias_value_pack_size(
                       &multi_forwarded_alias_value_pack_value) == 12 &&
                   incremented_forwarded_alias_value_pack_value.marker ==
                       109 &&
                   incremented_alias_value_pack_size(
                       &incremented_forwarded_alias_value_pack_value) == 8 &&
                   wrapped_alias_value_pack_size(
                       &wrapped_forwarded_alias_value_pack_value) == 12 &&
                   alias_value_pack_function_size<>() == 8 &&
                   alias_value_pack_function_size<1, 2, 3, 4, 5>() == 12 &&
                   enum_alias_value_pack_value.marker == 113 &&
                   enum_forwarded_alias_value_pack_value.marker == 127 &&
                   enum_alias_value_pack_size(
                       &enum_forwarded_alias_value_pack_value) == 8 &&
                   values[2] == 3 &&
                   class_alias_value == 47 &&
                   nested_class_alias_value.value == 61 &&
                   alias_without_template_keyword == 59 &&
                   namespace_class_alias_value == 53 &&
                   class_alias_array[2] == 5 &&
                   class_alias_local_value == 7 &&
                   protected_alias_value == 11
               ? 0 : 1;
}
