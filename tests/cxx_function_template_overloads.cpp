struct ConversionBaseRoot {};
struct ConversionBaseMiddle : ConversionBaseRoot {};
struct ConversionBaseLeaf : ConversionBaseMiddle {};

enum ConversionFixedByte : unsigned char {
    conversion_fixed_byte_value = 1
};

using ConversionThrowingFunctionPointer = void (*)();
using ConversionNoexceptFunctionPointer = void (*)() noexcept;
using ConversionExplicitThrowingFunctionPointer = void (*)() noexcept(false);
using ConversionExplicitNoexceptFunctionPointer = void (*)() noexcept(true);
using ConversionNoexceptIntPointerFunction = int (*)(int*) noexcept;
using ConversionThrowingIntPointerFunction = int (*)(int*);
using ConversionIntFunction = int (*)(int);
using ConversionIntPairFunction = int (*)(int, int);

struct ConversionMemberPointerBoolOwner {
    int value;

    bool is_positive() const { return value > 0; }
};

using ConversionDataMemberBoolPointer =
    int ConversionMemberPointerBoolOwner::*;
using ConversionFunctionMemberBoolPointer =
    bool (ConversionMemberPointerBoolOwner::*)() const;

double cxx_integer_return_as_double(int value) { return value; }
int cxx_free_variadic(int value, ...) { return value; }

template<typename T>
int cxx_free_variadic_template(T value, ...) { return value + 1; }

template<typename T>
int cxx_free_ellipsis_only_template(...) { return sizeof(T); }

int cxx_free_variadic_overload(int value, ...) { return value + 100; }

template<typename T>
int cxx_free_variadic_overload(T value, ...) { return value + 200; }

struct CxxEllipsisImplicitInt {
    int value;

    operator int() const { return value; }
};

struct CxxEllipsisExplicitInt {
    int value;

    explicit operator int() const { return value; }
};

int cxx_free_ellipsis_conversion_overload(int value) {
    return value + 3000;
}

int cxx_free_ellipsis_conversion_overload(...) { return -3000; }

int cxx_free_ellipsis_explicit_conversion_overload(int value) {
    return value + 4000;
}

int cxx_free_ellipsis_explicit_conversion_overload(...) { return 5000; }

struct CxxMemberEllipsisProbe {
    int select(int value) const { return value + 6000; }
    int select(...) const { return 7000; }
};

int cxx_free_fixed_overload(int value) { return value + 300; }

template<typename T>
int cxx_free_fixed_overload(T value) { return value + 400; }

int cxx_free_fixed_multi_overload(short value) { return value + 500; }

int cxx_free_fixed_multi_overload(int value) { return value + 600; }

template<typename T>
int cxx_free_fixed_multi_overload(T value) { return value + 700; }

struct CxxMixedOverloadBaseOne {};
struct CxxMixedOverloadBaseTwo {};
struct CxxMixedOverloadDerived : CxxMixedOverloadBaseOne,
                                 CxxMixedOverloadBaseTwo {};

int cxx_free_incomparable_overload(CxxMixedOverloadBaseOne*) { return 1; }

int cxx_free_incomparable_overload(CxxMixedOverloadBaseTwo*) { return 2; }

template<typename T>
int cxx_free_incomparable_overload(T*) { return 3; }

struct CxxVariadicAggregatePayload {
    int integer;
    double floating;
};

int cxx_free_variadic_payload_overload(int marker, ...) {
    __builtin_va_list arguments;
    __builtin_va_start(arguments, marker);
    CxxVariadicAggregatePayload payload =
        __builtin_va_arg(arguments, CxxVariadicAggregatePayload);
    __builtin_va_end(arguments);
    return marker + payload.integer + (int)payload.floating + 1000;
}

template<typename T>
int cxx_free_variadic_payload_overload(T marker, ...) {
    __builtin_va_list arguments;
    __builtin_va_start(arguments, marker);
    CxxVariadicAggregatePayload payload =
        __builtin_va_arg(arguments, CxxVariadicAggregatePayload);
    __builtin_va_end(arguments);
    return marker + payload.integer + (int)payload.floating + 2000;
}

void conversion_noexcept_function() noexcept {}
void conversion_explicit_throwing_function() noexcept(false) {}
void conversion_explicit_noexcept_function() noexcept(true) {}
void conversion_overloaded_noexcept_function(int) {}
void conversion_overloaded_noexcept_function() noexcept {}

template<typename T>
int conversion_function_template_overload(T) noexcept {
    return 1;
}

template<typename T>
int conversion_function_template_overload(T*) noexcept {
    return 2;
}

template<typename T>
int conversion_explicit_noexcept_function_template(T*) noexcept(true) {
    return 3;
}

template<typename T>
int conversion_dependent_noexcept_function_template(T*)
    noexcept(sizeof(T) == sizeof(int)) {
    return 4;
}

template<typename T>
int conversion_dependent_throwing_function_template(T*)
    noexcept(sizeof(T) != sizeof(int)) {
    return 5;
}

int conversion_non_template_preferred(int*) noexcept {
    return 6;
}

template<typename T>
int conversion_non_template_preferred(T*) noexcept {
    return 7;
}

int conversion_noexcept_template_rank(int*) {
    return 91;
}

template<typename T>
int conversion_noexcept_template_rank(T*) noexcept {
    return 92;
}

namespace ConversionFunctionTemplateNamespace {
template<typename T>
int select(T*) noexcept {
    return 8;
}
}

namespace ConversionFunctionTemplateImportedNamespace {
template<typename T>
int select_via_directive(T*) noexcept {
    return 81;
}

template<typename T>
int select_via_declaration(T*) noexcept {
    return 82;
}
}

namespace ConversionFunctionTemplateUsingDirectiveNamespace {
using namespace ConversionFunctionTemplateImportedNamespace;
}

namespace ConversionFunctionTemplateUsingDeclarationNamespace {
using ConversionFunctionTemplateImportedNamespace::select_via_declaration;
}

namespace CxxFunctionTemplateAdl {
struct Payload {
    int value;
};

long select_better_template(Payload* payload, long value) {
    return payload->value + (int)value + 500;
}

template<typename T>
int select_better_template(Payload* payload, T value) {
    return payload->value + value + 600;
}
}

namespace InlineNamespaceAssociatedAdl {
inline namespace v1 {
struct Token {};
}

template<typename T>
int select_inline_namespace_adl(Token, T value) {
    return value + 340;
}
}

struct AdlTiePayload {
    int value;
};

int adl_tie(AdlTiePayload* payload, int value) {
    return payload->value + value + 700;
}

template<typename T>
long adl_tie(AdlTiePayload* payload, T value) {
    return payload->value + value + 800;
}

template<typename... T>
int conversion_function_template_pack(T...) {
    return 6;
}

struct ConversionMemberPointerRoot {
    int value;

    int function(int parameter) const {
        return parameter;
    }

    void noexcept_function() noexcept {}

    void throwing_function() {}
};

struct ConversionNestedMemberPointerOwner {
    int* pointer;
};

struct ConversionMemberPointerMiddle : ConversionMemberPointerRoot {};
struct ConversionMemberPointerLeaf : ConversionMemberPointerMiddle {};

struct ConversionMemberPointerPrivateOwner
    : private ConversionMemberPointerRoot {
    friend int invoke_private_member_pointer_owner(
        int ConversionMemberPointerRoot::*);
    friend int invoke_private_member_pointer_owner_template(
        int ConversionMemberPointerRoot::*);
};

int choose_private_member_pointer_owner(
    int ConversionMemberPointerPrivateOwner::*) {
    return 243;
}

template<typename T>
int choose_private_member_pointer_owner_template(
    T, int ConversionMemberPointerPrivateOwner::*) {
    return 244;
}

int invoke_private_member_pointer_owner(
    int ConversionMemberPointerRoot::*member) {
    return choose_private_member_pointer_owner(member);
}

int invoke_private_member_pointer_owner_template(
    int ConversionMemberPointerRoot::*member) {
    return choose_private_member_pointer_owner_template(0, member);
}

struct ConversionValueRoot {
    int root_value;
};

struct ConversionValuePrefix {
    int prefix_value;
};

struct ConversionValueMiddle : ConversionValuePrefix,
                               ConversionValueRoot {
    int middle_value;
};

struct ConversionValueLeaf : ConversionValueMiddle {
    int leaf_value;
};

struct ConversionValueVirtualRoot {
    int root_value;
};

struct ConversionValueVirtualPrefix {
    int prefix_value;
};

struct ConversionValueVirtualPayload : ConversionValueVirtualPrefix,
                                       ConversionValueVirtualRoot {
    int payload_value;
};

struct ConversionValueVirtualMiddle : virtual ConversionValueVirtualPayload {
    int middle_value;
};

struct ConversionValueVirtualLeaf : ConversionValueVirtualMiddle {
    int leaf_value;
};

int conversion_value_cleanup_count = 0;

struct ConversionValueVirtualCleanupRoot {
    int root_value;
};

struct ConversionValueVirtualCleanupMiddle :
    virtual ConversionValueVirtualCleanupRoot {
    int middle_value;
};

struct ConversionValueVirtualCleanupLeaf :
    ConversionValueVirtualCleanupMiddle {
    int leaf_value;
    ~ConversionValueVirtualCleanupLeaf() {
        ++conversion_value_cleanup_count;
    }
};

struct ConversionValueCleanupRoot {
    int root_value;
};

struct ConversionValueCleanupPrefix {
    int prefix_value;
};

struct ConversionValueCleanupLeaf : ConversionValueCleanupPrefix,
                                    ConversionValueCleanupRoot {
    int leaf_value;
    ~ConversionValueCleanupLeaf() {
        ++conversion_value_cleanup_count;
    }
};

template<typename T>
int choose_template(T) {
    return 10;
}

template<typename T>
int choose_template(T* value) {
    return *value + 20;
}

template<typename T>
int choose_fixed_over_pack(T) {
    return 110;
}

template<typename... T>
int choose_fixed_over_pack(T...) {
    return 120;
}

template<typename T, typename... Rest>
int choose_fixed_prefix_over_pack(T, Rest...) {
    return 130;
}

template<typename... T>
int choose_fixed_prefix_over_pack(T...) {
    return 140;
}

template<typename... T>
int choose_pack_pointer_partial_order(T...) {
    return 145;
}

template<typename T, typename... Rest>
int choose_pack_pointer_partial_order(T*, Rest...) {
    return 146;
}

template<typename... Types>
int choose_pointer_expansion_partial_order(Types*...) {
    return 201;
}

template<typename First, typename... Rest>
int choose_pointer_expansion_partial_order(First*, Rest*...) {
    return 202;
}

template<typename... Rest>
int choose_nontrailing_function_pack(Rest..., int) {
    return 203;
}

template<typename... Rest>
int choose_nontrailing_pointer_function_pack(Rest*..., int tail) {
    return (int)sizeof...(Rest) + (int)tail;
}

template<typename... Rest, typename Tail>
int choose_nontrailing_pointer_pack_with_deduced_suffix(Rest*..., Tail tail) {
    return (int)sizeof...(Rest) + (int)tail;
}

template<typename... Rest, typename Tail = int>
int choose_nontrailing_pointer_pack_with_default_suffix(
    Rest*..., decltype(sizeof(Tail)) suffix) {
    return (int)sizeof...(Rest) * 100 + (int)sizeof(Tail) * 10 +
           (int)suffix;
}

template<typename... Types>
struct FunctionTemplatePackCountType {
    using type = char;
};

template<typename First, typename Second>
struct FunctionTemplatePackCountType<First, Second> {
    using type = short;
};

static_assert(sizeof(FunctionTemplatePackCountType<>::type) == 1);
static_assert(sizeof(FunctionTemplatePackCountType<int, long>::type) == 2);

template<int... Values>
struct FunctionTemplatePackValueType {
    using type = char;
};

template<int First, int Second>
struct FunctionTemplatePackValueType<First, Second> {
    using type = short;
};

static_assert(sizeof(FunctionTemplatePackValueType<>::type) == 1);
static_assert(sizeof(FunctionTemplatePackValueType<4, 2>::type) == 2);

template<typename Type>
struct FunctionTemplateTemplatePartialA {};

template<typename Type>
struct FunctionTemplateTemplatePartialB {};

template<template<typename> class... Templates>
struct FunctionTemplateTemplatePackPartialType {
    using type = char;
};

template<template<typename> class First, template<typename> class Second>
struct FunctionTemplateTemplatePackPartialType<First, Second> {
    using type = short;
};

static_assert(sizeof(FunctionTemplateTemplatePackPartialType<>::type) == 1);
static_assert(sizeof(FunctionTemplateTemplatePackPartialType<
                 FunctionTemplateTemplatePartialA,
                 FunctionTemplateTemplatePartialB>::type) == 2);

template<typename... Rest,
         typename Tail = typename FunctionTemplatePackCountType<
             Rest...>::type>
int choose_nontrailing_pointer_pack_with_pack_dependent_default_suffix(
    Rest*..., decltype(sizeof(Tail)) suffix) {
    return (int)sizeof...(Rest) * 100 + (int)sizeof(Tail) * 10 +
           (int)suffix;
}

template<typename... Rest,
         typename Tail = typename FunctionTemplatePackValueType<
             static_cast<int>(sizeof(Rest))...>::type>
int choose_nontrailing_pointer_pack_with_non_type_pack_dependent_default_suffix(
    Rest*..., decltype(sizeof(Tail)) suffix) {
    return (int)sizeof...(Rest) * 100 + (int)sizeof(Tail) * 10 +
           (int)suffix;
}

template<template<typename> class... Templates,
         typename Tail = typename FunctionTemplateTemplatePackPartialType<
             Templates...>::type>
int choose_template_template_pack_with_partial_default_suffix(
    decltype(sizeof(Tail)) suffix) {
    return (int)sizeof...(Templates) * 100 + (int)sizeof(Tail) * 10 +
           (int)suffix;
}

template<int Value>
struct FunctionTemplateValuePackTag {};

template<int... Values>
struct FunctionTemplateNonTypePackBox {};

template<short Value>
struct FunctionTemplateShortValuePackTag {};

template<int... Values>
int choose_non_type_pack_partial_order(
    FunctionTemplateValuePackTag<Values>...) {
    return 170;
}

template<int Value, int... Rest>
int choose_non_type_pack_partial_order(
    FunctionTemplateValuePackTag<Value>,
    FunctionTemplateValuePackTag<Rest>...) {
    return 171;
}

template<int... Values>
int choose_non_type_class_pack_partial_order(
    FunctionTemplateNonTypePackBox<Values...>) {
    return 182;
}

template<int Value, int... Rest>
int choose_non_type_class_pack_partial_order(
    FunctionTemplateNonTypePackBox<Value, Rest...>) {
    return 183;
}

template<int Value>
struct FunctionTemplateExpressionValuePackTag {};

template<int... Values>
int choose_expression_non_type_pack_partial_order(
    FunctionTemplateExpressionValuePackTag<(Values + 1)>...) {
    return 180;
}

template<int Value, int... Rest>
int choose_expression_non_type_pack_partial_order(
    FunctionTemplateExpressionValuePackTag<(Value + 1)>,
    FunctionTemplateExpressionValuePackTag<(Rest + 1)>...) {
    return 181;
}

template<int First, int Second>
struct FunctionTemplateTwoValueTag {};

template<typename T>
struct FunctionTemplatePartialOrderBox {};

template<typename... Types>
struct FunctionTemplatePackPartialOrderBox {};

template<template<typename> class... Templates>
struct FunctionTemplateTemplatePackBox {};

template<typename Type>
struct FunctionTemplateTemplatePackItem {};

template<typename Type>
struct FunctionTemplateTemplatePackFixed {};

template<typename Type>
struct FunctionTemplatePackCompoundElement {};

template<typename First, typename Second>
struct FunctionTemplatePackRepeatedPair {};

template<int Id, typename Type>
struct FunctionTemplateMixedValueBox {};

template<int Id, typename... Types>
struct FunctionTemplateMixedValuePackBox {};

template<int Size, typename Head, typename... Types>
struct FunctionTemplateDependentValuePackBox {};

template<int Size, typename Head, typename... Types>
struct FunctionTemplateArithmeticValuePackBox {};

template<int Value, typename Head, typename... Types>
struct FunctionTemplateMemberValuePackBox {};

struct FunctionTemplateMemberValueOne {
    static constexpr int value = 1;
};

struct FunctionTemplateMemberValueTwo {
    static constexpr int value = 2;
};

struct FunctionTemplateMemberValueThree {
    static constexpr int value = 3;
};

struct FunctionTemplateMemberCallOne {
    static constexpr int value() { return 1; }
};

struct FunctionTemplateMemberCallTwo {
    static constexpr int value() { return 2; }
};

struct FunctionTemplateMemberCallNonConstexpr {
    static int value() { return 1; }
};

struct FunctionTemplateMemberCallPrivate {
private:
    static constexpr int value() { return 1; }
};

struct FunctionTemplateMemberCallArgumentOne {
    static constexpr int value(int argument) { return argument - 1; }
};

struct FunctionTemplateMemberCallArgumentTwo {
    static constexpr int value(int argument) { return argument + 1; }
};

struct FunctionTemplateMemberCallArgumentPair {
    static constexpr int value(int first, int second) {
        return first * 10 + second;
    }
    static constexpr int value(short first, int second) {
        return first * 10 + second + 1000;
    }
    static constexpr int value(int first, short second) {
        return first * 10 + second + 2000;
    }
};

struct FunctionTemplateMemberCallOverloaded {
    static constexpr int value() { return 9; }
    static constexpr int value(int argument) { return argument + 1; }
    static constexpr int value(short argument) { return argument + 2; }
};

struct FunctionTemplateMemberCallDefaulted {
    static constexpr int value(short argument, int increment = 1) {
        return argument + increment;
    }
    static constexpr int value(int argument) { return argument + 100; }
};

struct FunctionTemplateMemberCallNoExplicitArguments {
    static constexpr int value(int tens = 2, int units = 5) {
        return tens * 10 + units;
    }
};

struct FunctionTemplateMemberCallNoExplicitArgumentsOne {
    static constexpr int value(int tens = 0, int units = 1) {
        return tens * 10 + units;
    }
};

struct FunctionTemplateMemberCallConstReference {
    static constexpr int value(const int& argument) {
        return argument + 1;
    }
};

struct FunctionTemplateMemberCallRvalueReference {
    static constexpr int value(int&& argument) {
        return argument + 1;
    }
};

struct FunctionTemplateMemberCallTemplate {
    template<typename T>
    static constexpr int value(T argument) {
        return argument + 1;
    }
};

struct FunctionTemplateMemberCallFloatingOverload {
    static constexpr int value(int argument) {
        return argument + 10;
    }

    static constexpr int value(double argument) {
        return argument + 20;
    }
};

struct FunctionTemplateMemberCallFloatingToInteger {
    static constexpr int value(int argument) {
        return argument + 10;
    }
};

struct FunctionTemplateMemberCallNonTypePack {
    template<int... Values>
    static constexpr int value() {
        return (Values + ... + 0);
    }
};

struct FunctionTemplateMemberCallMixedOverload {
    static constexpr int value(int argument) {
        return argument + 1;
    }

    template<typename T>
    static constexpr int value(T argument) {
        return argument + 2;
    }
};

struct FunctionTemplateMemberCallVariadicCompetitor {
    static constexpr int value(int first, int second) {
        return first + second;
    }

    static int value(int first, ...) {
        return first + 99;
    }

    static int value(...) {
        return 99;
    }

    template<typename First, typename Second>
    static constexpr int value(First first, Second second) {
        return first + second + 100;
    }
};

struct FunctionTemplateMemberCallVariadicOrdinary {
    static constexpr int value(int first, int second) {
        return first + second;
    }

    static int value(int first, ...) {
        return first + 99;
    }

    static int value(...) {
        return 99;
    }
};

struct FunctionTemplateMemberCallVariadicMemberTemplate {
    template<typename T>
    static constexpr int value(T first, ...) {
        return first + 1;
    }
};

struct FunctionTemplateMemberCallVariadicMemberTemplateCompetitor {
    static constexpr int value(int first, int second) {
        return first + second;
    }

    template<typename T>
    static constexpr int value(T first, ...) {
        return first + 100;
    }
};

struct FunctionTemplateMemberCallOrdinaryFallback {
    static constexpr int value(int argument) {
        return argument + 1;
    }

    template<typename T>
    static constexpr int value(T* argument) {
        return *argument + 2;
    }
};

struct FunctionTemplateMemberCallNonIntegerOverload {
    static constexpr int value(int argument) {
        return argument + 1;
    }

    static constexpr int value(double) {
        return 99;
    }

    template<typename T>
    static constexpr int value(T* argument) {
        return *argument + 2;
    }
};

struct FunctionTemplateMemberCallInheritedBase {
    static constexpr int value(int argument) {
        return argument + 1;
    }
};

struct FunctionTemplateMemberCallInheritedDerived
    : FunctionTemplateMemberCallInheritedBase {};

struct FunctionTemplateMemberCallInheritedTemplateBase {
    int base_value;

    template<typename T>
    static constexpr int value(T argument) {
        return argument + 1;
    }

    template<typename T>
    int adjust(T argument) {
        return base_value + argument;
    }
};

struct FunctionTemplateMemberCallInheritedTemplateDerived
    : FunctionTemplateMemberCallInheritedTemplateBase {};

struct FunctionTemplateMemberCallInheritedTemplateVirtualDerived
    : virtual FunctionTemplateMemberCallInheritedTemplateBase {};

struct FunctionTemplateMemberCallInheritedTemplatePadding {
    int padding;
};

struct FunctionTemplateMemberCallInheritedTemplateMultipleDerived
    : FunctionTemplateMemberCallInheritedTemplatePadding,
      FunctionTemplateMemberCallInheritedTemplateBase {};

struct FunctionTemplateMemberCallInheritedTemplateOtherBase {
    int other_value;

    template<typename T>
    int adjust(T argument) {
        return other_value + argument + 100;
    }
};

struct FunctionTemplateMemberCallInheritedTemplateUsingDerived
    : FunctionTemplateMemberCallInheritedTemplatePadding,
      FunctionTemplateMemberCallInheritedTemplateBase,
      FunctionTemplateMemberCallInheritedTemplateOtherBase {
    using FunctionTemplateMemberCallInheritedTemplateBase::adjust;
};

struct FunctionTemplateMemberCallArgumentNonConstexpr {
    static int value(int argument) { return argument - 1; }
};

struct FunctionTemplateMemberCallArgumentPrivate {
private:
    static constexpr int value(int argument) { return argument - 1; }
};

template<int Value>
struct FunctionTemplateMemberCallArgumentTag {
    static constexpr int value = Value;
};

template<typename T>
int choose_template_pack_partial_order(
    FunctionTemplatePackPartialOrderBox<T>) {
    return 197;
}

template<typename T>
int choose_template_pack_partial_order(
    FunctionTemplatePackPartialOrderBox<T*>) {
    return 198;
}

template<typename First>
int choose_template_class_pack_partial_order(
    FunctionTemplatePackPartialOrderBox<First, int>) {
    return 199;
}

template<typename First>
int choose_template_class_pack_partial_order(
    FunctionTemplatePackPartialOrderBox<First*, int>) {
    return 200;
}

template<typename First, typename... Rest>
int count_template_class_pack_expansion(
    FunctionTemplatePackPartialOrderBox<First, Rest...>) {
    return 210 + sizeof...(Rest);
}

template<typename... Types>
int choose_class_template_pack_expansion_order(
    FunctionTemplatePackPartialOrderBox<Types...>) {
    return 213;
}

template<typename First, typename... Rest>
int choose_class_template_pack_expansion_order(
    FunctionTemplatePackPartialOrderBox<First, Rest...>) {
    return 214;
}

template<template<typename> class... Templates>
int choose_class_template_template_pack_expansion_order(
    FunctionTemplateTemplatePackBox<Templates...>) {
    return 276;
}

template<template<typename> class First,
         template<typename> class... Rest>
int choose_class_template_template_pack_expansion_order(
    FunctionTemplateTemplatePackBox<First, Rest...>) {
    return 277;
}

template<typename... Types>
int choose_nontrailing_template_argument_pack_order(
    FunctionTemplatePackPartialOrderBox<Types..., int>) {
    return 268;
}

template<typename... Types>
int choose_nontrailing_template_argument_pack_order(
    FunctionTemplatePackPartialOrderBox<Types...>) {
    return 269;
}

template<typename... Types>
int choose_repeated_class_template_pack_expansions(
    FunctionTemplatePackPartialOrderBox<Types..., int, Types...>,
    FunctionTemplatePackPartialOrderBox<Types...>) {
    return static_cast<int>(sizeof...(Types));
}

template<int... Values>
int choose_repeated_class_non_type_pack_expansions(
    FunctionTemplateNonTypePackBox<Values..., 7, Values...>,
    FunctionTemplateNonTypePackBox<Values...>) {
    return static_cast<int>(sizeof...(Values));
}

template<template<typename> class... Templates>
int choose_repeated_class_template_template_pack_expansions(
    FunctionTemplateTemplatePackBox<
        Templates..., FunctionTemplateTemplatePackFixed, Templates...>,
    FunctionTemplateTemplatePackBox<Templates...>) {
    return static_cast<int>(sizeof...(Templates));
}

template<typename... FirstTypes, typename... SecondTypes>
int choose_interleaved_class_template_pack_sources(
    FunctionTemplatePackPartialOrderBox<FirstTypes..., int, SecondTypes...>,
    FunctionTemplatePackPartialOrderBox<SecondTypes...>) {
    return static_cast<int>(sizeof...(FirstTypes) * 10 +
                            sizeof...(SecondTypes));
}

template<typename... FirstTypes, typename... SecondTypes>
int choose_independent_class_template_pack_partition(
    FunctionTemplatePackPartialOrderBox<FirstTypes...>,
    FunctionTemplatePackPartialOrderBox<SecondTypes...>) {
    return static_cast<int>(sizeof...(FirstTypes) * 10 +
                            sizeof...(SecondTypes));
}

template<int... FirstValues, int... SecondValues>
int choose_independent_non_type_class_pack_partition(
    FunctionTemplateNonTypePackBox<FirstValues...>,
    FunctionTemplateNonTypePackBox<SecondValues...>) {
    return static_cast<int>(sizeof...(FirstValues) * 10 +
                            sizeof...(SecondValues));
}

template<template<typename> class... FirstTemplates,
         template<typename> class... SecondTemplates>
int choose_independent_template_template_class_pack_partition(
    FunctionTemplateTemplatePackBox<FirstTemplates...>,
    FunctionTemplateTemplatePackBox<SecondTemplates...>) {
    return static_cast<int>(sizeof...(FirstTemplates) * 10 +
                            sizeof...(SecondTemplates));
}

template<int... Values>
int choose_nontrailing_non_type_pack_order(
    FunctionTemplateNonTypePackBox<Values..., 7>) {
    return 272;
}

template<int... Values>
int choose_nontrailing_non_type_pack_order(
    FunctionTemplateNonTypePackBox<Values...>) {
    return 273;
}

template<template<typename> class... Templates>
int choose_nontrailing_template_template_pack_order(
    FunctionTemplateTemplatePackBox<
        Templates..., FunctionTemplateTemplatePackFixed>) {
    return 274;
}

template<template<typename> class... Templates>
int choose_nontrailing_template_template_pack_order(
    FunctionTemplateTemplatePackBox<Templates...>) {
    return 275;
}

template<typename... Types>
int count_class_pointer_pack_expansion(
    FunctionTemplatePackPartialOrderBox<Types*...>) {
    return 220 + sizeof...(Types);
}

template<typename... Types>
int choose_compound_pointer_pack_order(
    FunctionTemplatePackPartialOrderBox<Types*...>) {
    return 223;
}

template<typename First, typename... Rest>
int choose_compound_pointer_pack_order(
    FunctionTemplatePackPartialOrderBox<First*, Rest...>) {
    return 224;
}

template<typename... Types>
int choose_compound_pointer_over_plain_pack(
    FunctionTemplatePackPartialOrderBox<Types*...>) {
    return 225;
}

template<typename... Types>
int choose_compound_pointer_over_plain_pack(
    FunctionTemplatePackPartialOrderBox<Types...>) {
    return 226;
}

template<typename... Types>
int choose_compound_class_pack_order(
    FunctionTemplatePackPartialOrderBox<
        FunctionTemplatePackCompoundElement<Types>...>) {
    return 260;
}

template<typename... Types>
int choose_compound_class_pack_order(
    FunctionTemplatePackPartialOrderBox<Types...>) {
    return 261;
}

template<typename... Types>
int choose_repeated_compound_class_pack_order(
    FunctionTemplatePackPartialOrderBox<
        FunctionTemplatePackRepeatedPair<Types, Types>...>) {
    return 262;
}

template<typename... Types>
int choose_repeated_compound_class_pack_order(
    FunctionTemplatePackPartialOrderBox<Types...>) {
    return 263;
}

template<typename Head, typename... Types>
int choose_compound_pack_size_order(
    FunctionTemplateDependentValuePackBox<
        sizeof...(Types), Head,
        FunctionTemplatePackCompoundElement<Types>...>) {
    return 264;
}

template<typename Head, typename... Types>
int choose_compound_pack_size_order(
    FunctionTemplateDependentValuePackBox<sizeof...(Types), Head, Types...>) {
    return 265;
}

template<typename Head, typename... Types>
int choose_compound_pack_size_plus_one_order(
    FunctionTemplateDependentValuePackBox<
        sizeof...(Types) + 1, Head,
        FunctionTemplatePackCompoundElement<Types>...>) {
    return 266;
}

template<typename Head, typename... Types>
int choose_compound_pack_size_plus_one_order(
    FunctionTemplateDependentValuePackBox<sizeof...(Types) + 1, Head,
                                          Types...>) {
    return 267;
}

template<typename Type>
int choose_mixed_value_class_order(
    FunctionTemplateMixedValueBox<7, Type>) {
    return 227;
}

template<typename Type>
int choose_mixed_value_class_order(
    FunctionTemplateMixedValueBox<7, Type*>) {
    return 228;
}

template<typename... Types>
int choose_mixed_value_class_pack_order(
    FunctionTemplateMixedValuePackBox<7, Types...>) {
    return 229;
}

template<typename... Types>
int choose_mixed_value_class_pack_order(
    FunctionTemplateMixedValuePackBox<7, Types*...>) {
    return 230;
}

template<typename Head, typename... Types>
int choose_dependent_value_class_pack_order(
    FunctionTemplateDependentValuePackBox<sizeof(Head), Head, Types...>) {
    return 231;
}

template<typename Head, typename... Types>
int choose_dependent_value_class_pack_order(
    FunctionTemplateDependentValuePackBox<sizeof(Head), Head, Types*...>) {
    return 232;
}

template<typename Head, typename... Types>
int choose_pack_size_class_pack_order(
    FunctionTemplateDependentValuePackBox<sizeof...(Types), Head, Types...>) {
    return 241;
}

template<typename Head, typename... Types>
int choose_pack_size_class_pack_order(
    FunctionTemplateDependentValuePackBox<2, Head, Types...>) {
    return 242;
}

template<typename Head, typename... Types>
int choose_arithmetic_value_class_pack_order(
    FunctionTemplateArithmeticValuePackBox<sizeof(Head) + 1, Head,
                                           Types...>) {
    return 233;
}

template<typename Head, typename... Types>
int choose_arithmetic_value_class_pack_order(
    FunctionTemplateArithmeticValuePackBox<sizeof(Head) + 1, Head,
                                           Types*...>) {
    return 234;
}

template<typename Head, typename... Types>
int choose_member_value_class_pack_order(
    FunctionTemplateMemberValuePackBox<Head::value, Head, Types...>) {
    return 235;
}

template<typename Head, typename... Types>
int choose_member_value_class_pack_order(
    FunctionTemplateMemberValuePackBox<1, Head, Types...>) {
    return 236;
}

template<typename Head, typename... Types>
int choose_member_call_value_class_pack_order(
    FunctionTemplateMemberValuePackBox<Head::value(), Head, Types...>) {
    return 243;
}

template<typename Head, typename... Types>
int choose_member_call_value_class_pack_order(
    FunctionTemplateMemberValuePackBox<1, Head, Types...>) {
    return 244;
}

template<typename Head, typename... Types>
int choose_member_call_argument_class_pack_order(
    FunctionTemplateMemberValuePackBox<Head::value(2), Head, Types...>) {
    return 245;
}

template<typename Head, typename... Types>
int choose_member_call_argument_class_pack_order(
    FunctionTemplateMemberValuePackBox<1, Head, Types...>) {
    return 246;
}

template<typename Head, typename ArgumentTag, typename... Types>
int choose_member_call_dependent_argument_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::value(ArgumentTag::value), Head, ArgumentTag, Types...>) {
    return 247;
}

template<typename Head, typename ArgumentTag, typename... Types>
int choose_member_call_dependent_argument_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        1, Head, ArgumentTag, Types...>) {
    return 248;
}

template<typename Head, int Argument, typename... Types>
int choose_member_call_nontype_argument_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::value(Argument), Head,
        FunctionTemplateValuePackTag<Argument>, Types...>) {
    return 249;
}

template<int Value, typename Item, typename... Remaining>
int choose_member_call_nontype_argument_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        1, Item, FunctionTemplateValuePackTag<Value>, Remaining...>) {
    return 250;
}

template<typename Head, int First, int Second, typename... Types>
int choose_member_call_multiple_nontype_arguments_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::value(First, Second), Head,
        FunctionTemplateValuePackTag<First>,
        FunctionTemplateValuePackTag<Second>, Types...>) {
    return 251;
}

template<int SecondValue, typename Item, int FirstValue,
         typename... Remaining>
int choose_member_call_multiple_nontype_arguments_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        1, Item, FunctionTemplateValuePackTag<FirstValue>,
        FunctionTemplateValuePackTag<SecondValue>, Remaining...>) {
    return 252;
}

template<typename Head, int Argument, typename... Types>
int choose_member_call_overloaded_nontype_argument_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::value(Argument), Head,
        FunctionTemplateValuePackTag<Argument>, Types...>) {
    return 253;
}

template<int Value, typename Item, typename... Remaining>
int choose_member_call_overloaded_nontype_argument_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        1, Item, FunctionTemplateValuePackTag<Value>, Remaining...>) {
    return 254;
}

template<typename Head, short Argument, typename... Types>
int choose_member_call_defaulted_nontype_argument_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::value(Argument), Head,
        FunctionTemplateShortValuePackTag<Argument>, Types...>) {
    return 255;
}

template<short Value, typename Item, typename... Remaining>
int choose_member_call_defaulted_nontype_argument_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        1, Item, FunctionTemplateShortValuePackTag<Value>, Remaining...>) {
    return 256;
}

template<typename Head, typename... Types>
int choose_member_call_no_explicit_arguments_class_pack_order(
    FunctionTemplateMemberValuePackBox<Head::value(), Head, Types...>) {
    return 257;
}

template<int Value, typename Item, typename... Remaining>
int choose_member_call_no_explicit_arguments_class_pack_order(
    FunctionTemplateMemberValuePackBox<1, Item, Remaining...>) {
    return 258;
}

template<typename Head, int Argument, typename... Types>
int choose_member_call_const_reference_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::value(Argument), Head,
        FunctionTemplateValuePackTag<Argument>, Types...>) {
    return 259;
}

template<typename Item, int Argument, typename... Remaining>
int choose_member_call_const_reference_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        1, Item, FunctionTemplateValuePackTag<Argument>, Remaining...>) {
    return 260;
}

template<typename Head, int Argument, typename... Types>
int choose_member_call_rvalue_reference_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::value(Argument), Head,
        FunctionTemplateValuePackTag<Argument>, Types...>) {
    return 261;
}

template<typename Item, int Argument, typename... Remaining>
int choose_member_call_rvalue_reference_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        1, Item, FunctionTemplateValuePackTag<Argument>, Remaining...>) {
    return 262;
}

template<typename Head, int Argument, typename... Types>
int choose_member_call_template_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::value(Argument), Head,
        FunctionTemplateValuePackTag<Argument>, Types...>) {
    return 263;
}

template<typename Item, int Argument, typename... Remaining>
int choose_member_call_template_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        1, Item, FunctionTemplateValuePackTag<Argument>, Remaining...>) {
    return 264;
}

template<typename Head, int Argument, typename... Types>
int choose_member_call_template_explicit_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::template value<int>(Argument), Head,
        FunctionTemplateValuePackTag<Argument>, Types...>) {
    return 265;
}

template<typename Item, int Argument, typename... Remaining>
int choose_member_call_template_explicit_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        1, Item, FunctionTemplateValuePackTag<Argument>, Remaining...>) {
    return 266;
}

template<typename Head, int Argument, typename... Types>
int choose_member_call_template_double_explicit_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::template value<double>(Argument), Head,
        FunctionTemplateValuePackTag<Argument>, Types...>) {
    return 277;
}

template<int Value, typename Item, typename... Remaining>
int choose_member_call_template_double_explicit_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        1, Item, FunctionTemplateValuePackTag<Value>, Remaining...>) {
    return 278;
}

template<typename Head, typename... Types>
int choose_member_call_floating_template_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::template value<double>(2.5), Head, Types...>) {
    return 279;
}

template<typename Item, typename... Remaining>
int choose_member_call_floating_template_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        1, Item, Remaining...>) {
    return 280;
}

template<typename Head, typename... Types>
int choose_member_call_floating_overload_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::value(0.5f), Head, Types...>) {
    return 281;
}

template<typename Item, typename... Remaining>
int choose_member_call_floating_overload_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        1, Item, Remaining...>) {
    return 282;
}

template<typename Head, typename... Types>
int choose_member_call_floating_to_integer_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::value(2.75f), Head, Types...>) {
    return 283;
}

template<typename Item, typename... Remaining>
int choose_member_call_floating_to_integer_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        1, Item, Remaining...>) {
    return 284;
}

template<typename Head, int... Values>
int choose_member_call_nontype_pack_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::template value<Values...>(), Head,
        FunctionTemplateValuePackTag<Values>...>) {
    return 285;
}

template<typename Head, typename... Tags>
int choose_member_call_nontype_pack_class_pack_order(
    FunctionTemplateMemberValuePackBox<1, Head, Tags...>) {
    return 286;
}

template<typename Head, int... Values>
int choose_member_call_nontype_pack_expression_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::template value<(Values + 1)...>(), Head,
        FunctionTemplateValuePackTag<Values>...>) {
    return 287;
}

template<typename Head, typename... Tags>
int choose_member_call_nontype_pack_expression_class_pack_order(
    FunctionTemplateMemberValuePackBox<1, Head, Tags...>) {
    return 288;
}

template<typename Head, int Argument, typename... Types>
int choose_member_call_mixed_overload_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::value(Argument), Head,
        FunctionTemplateValuePackTag<Argument>, Types...>) {
    return 267;
}

template<typename Item, int Argument, typename... Remaining>
int choose_member_call_mixed_overload_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        3, Item, FunctionTemplateValuePackTag<Argument>, Remaining...>) {
    return 268;
}

template<typename Head, typename... Types>
int choose_member_call_variadic_competitor_class_pack_order(
    FunctionTemplateMemberValuePackBox<Head::value(2, 3), Head, Types...>) {
    return 289;
}

template<typename Item, typename... Types>
int choose_member_call_variadic_competitor_class_pack_order(
    FunctionTemplateMemberValuePackBox<1, Item, Types...>) {
    return 290;
}

template<typename Head, typename... Types>
int choose_member_call_variadic_ordinary_class_pack_order(
    FunctionTemplateMemberValuePackBox<Head::value(2, 3), Head, Types...>) {
    return 291;
}

template<typename Item, typename... Types>
int choose_member_call_variadic_ordinary_class_pack_order(
    FunctionTemplateMemberValuePackBox<1, Item, Types...>) {
    return 292;
}

template<typename Head, typename... Types>
int choose_member_call_variadic_member_template_class_pack_order(
    FunctionTemplateMemberValuePackBox<Head::value(2, 3), Head, Types...>) {
    return 293;
}

template<typename Item, typename... Types>
int choose_member_call_variadic_member_template_class_pack_order(
    FunctionTemplateMemberValuePackBox<1, Item, Types...>) {
    return 294;
}

template<typename Head, typename... Types>
int choose_member_call_floating_variadic_member_template_class_pack_order(
    FunctionTemplateMemberValuePackBox<Head::value(2, 3.5f), Head, Types...>) {
    return 297;
}

template<typename Item, typename... Types>
int choose_member_call_floating_variadic_member_template_class_pack_order(
    FunctionTemplateMemberValuePackBox<1, Item, Types...>) {
    return 298;
}

template<typename Head, typename... Types>
int choose_member_call_aggregate_variadic_member_template_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::value(2, CxxVariadicAggregatePayload{3, 4.5}),
        Head, Types...>) {
    return 299;
}

template<typename Item, typename... Types>
int choose_member_call_aggregate_variadic_member_template_class_pack_order(
    FunctionTemplateMemberValuePackBox<1, Item, Types...>) {
    return 300;
}

template<typename Head, typename... Types>
int choose_member_call_variadic_member_template_competitor_class_pack_order(
    FunctionTemplateMemberValuePackBox<Head::value(2, 3), Head, Types...>) {
    return 295;
}

template<typename Item, typename... Types>
int choose_member_call_variadic_member_template_competitor_class_pack_order(
    FunctionTemplateMemberValuePackBox<1, Item, Types...>) {
    return 296;
}

template<typename Head, int Argument, typename... Types>
int choose_member_call_noninteger_competitor_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::value(Argument), Head,
        FunctionTemplateValuePackTag<Argument>, Types...>) {
    return 269;
}

template<typename Item, int Argument, typename... Remaining>
int choose_member_call_noninteger_competitor_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        3, Item, FunctionTemplateValuePackTag<Argument>, Remaining...>) {
    return 270;
}

template<typename Head, int Argument, typename... Types>
int choose_inherited_member_call_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::value(Argument), Head,
        FunctionTemplateValuePackTag<Argument>, Types...>) {
    return 271;
}

template<typename Item, int Argument, typename... Remaining>
int choose_inherited_member_call_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        3, Item, FunctionTemplateValuePackTag<Argument>, Remaining...>) {
    return 272;
}

template<typename Head, int Argument, typename... Types>
int choose_inherited_member_call_template_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::value(Argument), Head,
        FunctionTemplateValuePackTag<Argument>, Types...>) {
    return 273;
}

template<typename Item, int Argument, typename... Remaining>
int choose_inherited_member_call_template_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        4, Item, FunctionTemplateValuePackTag<Argument>, Remaining...>) {
    return 274;
}

template<typename Head, int Argument, typename... Types>
int choose_inherited_member_call_template_explicit_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        Head::template value<int>(Argument), Head,
        FunctionTemplateValuePackTag<Argument>, Types...>) {
    return 275;
}

template<typename Item, int Argument, typename... Remaining>
int choose_inherited_member_call_template_explicit_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        4, Item, FunctionTemplateValuePackTag<Argument>, Remaining...>) {
    return 276;
}

template<typename Head, typename... Types>
int choose_member_arithmetic_value_class_pack_order(
    FunctionTemplateMemberValuePackBox<Head::value + 1, Head, Types...>) {
    return 237;
}

template<typename Head, typename... Types>
int choose_member_arithmetic_value_class_pack_order(
    FunctionTemplateMemberValuePackBox<2, Head, Types...>) {
    return 238;
}

template<typename Head, typename... Types>
int choose_member_conditional_value_class_pack_order(
    FunctionTemplateMemberValuePackBox<
        (Head::value == 3 ? 5 : Head::value + 1), Head, Types...>) {
    return 239;
}

template<typename Head, typename... Types>
int choose_member_conditional_value_class_pack_order(
    FunctionTemplateMemberValuePackBox<2, Head, Types...>) {
    return 240;
}

template<template<typename> class Template, typename T>
int choose_template_template_partial_order(Template<T>) {
    return 194;
}

template<typename T>
int choose_template_template_partial_order(
    FunctionTemplatePartialOrderBox<T>) {
    return 195;
}

template<typename T>
int choose_template_template_partial_order(
    FunctionTemplatePartialOrderBox<T*>) {
    return 196;
}

template<int First, int Second>
int choose_dependent_non_type_partial_order(
    FunctionTemplateTwoValueTag<First, Second>) {
    return 190;
}

template<int Value>
int choose_dependent_non_type_partial_order(
    FunctionTemplateTwoValueTag<Value, Value>) {
    return 191;
}

template<int First, int Second>
int choose_fixed_non_type_partial_order(
    FunctionTemplateTwoValueTag<First, Second>) {
    return 192;
}

template<int First>
int choose_fixed_non_type_partial_order(
    FunctionTemplateTwoValueTag<First, 7>) {
    return 193;
}

template<typename T, typename... Rest>
int choose_pointer_pack_over_value(T*, Rest...) {
    return 147;
}

template<typename T>
int choose_pointer_pack_over_value(T) {
    return 148;
}

template<typename T>
concept PackOrderHasValue = requires(T candidate) {
    candidate.value;
};

template<typename T>
concept PackOrderHasValueAndSize =
    PackOrderHasValue<T> && (sizeof(T) >= sizeof(int));

template<typename T>
concept PackOrderHasPositiveSize = sizeof(T) >= 1;

template<int Value>
concept PackOrderPositiveNonType = Value > 0;

constexpr int pack_order_constexpr_default_value(int value) {
    return 3 * value + 2;
}

constexpr int pack_order_constexpr_default_value(long value) {
    return 3 * value + 4;
}

template<typename T>
constexpr int pack_order_constexpr_template_default_value(T value) {
    return 3 * value + 2;
}

template<typename T>
concept PackOrderConstexprDefaultInt = sizeof(T) == sizeof(int);

template<typename T>
constexpr int pack_order_overloaded_constexpr_template_default_value(
    T value) {
    return 3 * value + 4;
}

template<typename T>
    requires PackOrderConstexprDefaultInt<T>
constexpr int pack_order_overloaded_constexpr_template_default_value(
    T value) {
    return 3 * value + 2;
}

namespace PackOrderConstexprUsingLookupCallee {
constexpr int default_value(int value) {
    return 3 * value + 2;
}

constexpr int default_value(long value) {
    return 3 * value + 4;
}
}

namespace PackOrderConstexprUsingLookupTemplate {
using PackOrderConstexprUsingLookupCallee::default_value;

template<int N, typename T,
         template<typename, int = default_value(N)> class Box>
    requires ::PackOrderHasValueAndSize<T>
int choose_using_lookup_constexpr_call_default_order(
    Box<T>, T (&values)[N]) {
    return values[0].value == 3 ? 223 : 224;
}

template<int N, typename T, template<typename, int> class Box>
    requires ::PackOrderHasValue<T>
int choose_using_lookup_constexpr_call_default_order(
    Box<T, default_value(N)>, T (&values)[N]) {
    return values[0].value == 3 ? 225 : 226;
}
}

namespace PackOrderConstexprUsingDirectiveSource {
inline namespace InlineOverloads {
constexpr int default_value(int value) {
    return 3 * value + 2;
}

constexpr int default_value(long value) {
    return 3 * value + 4;
}
}
}

namespace PackOrderConstexprUsingDirectiveCallee {
using namespace PackOrderConstexprUsingDirectiveSource;
}

namespace PackOrderConstexprUsingDirectiveTemplate {
using namespace PackOrderConstexprUsingDirectiveCallee;

template<int N, typename T,
         template<typename, int = default_value(N)> class Box>
    requires ::PackOrderHasValueAndSize<T>
int choose_using_directive_constexpr_call_default_order(
    Box<T>, T (&values)[N]) {
    return values[0].value == 3 ? 227 : 228;
}

template<int N, typename T, template<typename, int> class Box>
    requires ::PackOrderHasValue<T>
int choose_using_directive_constexpr_call_default_order(
    Box<T, default_value(N)>, T (&values)[N]) {
    return values[0].value == 3 ? 229 : 230;
}
}

namespace PackOrderConstexprEnumAdl {
enum Value { seed = 2 };

constexpr int default_value(Value value) {
    return value == seed ? 12 : 14;
}
}

namespace PackOrderConstexprInlineEnumAdl {
inline namespace V1 {
enum Value { inline_seed = 2 };
}

constexpr int default_value(Value value) {
    return value == inline_seed ? 12 : 14;
}
}

namespace PackOrderConstexprInlineEnumTemplateAdl {
inline namespace V1 {
enum Value { inline_template_seed = 3 };
}

template<typename T>
constexpr int inline_template_default_value(T value) {
    return value == inline_template_seed ? 21 : 23;
}
}

using PackOrderConstexprEnumAdl::Value;
using PackOrderConstexprEnumAdl::seed;
using PackOrderConstexprInlineEnumAdl::inline_seed;
using PackOrderConstexprInlineEnumTemplateAdl::inline_template_seed;

static_assert(default_value(seed) == 12,
              "ordinary calls must find enum-namespace constexpr functions by ADL");
static_assert(inline_template_default_value(inline_template_seed) == 21,
              "ADL must find enum-associated constexpr function templates");

namespace PackOrderConstexprEnumAdlTemplate {
template<Value V, typename T,
         template<typename, int = default_value(V)> class Box>
constexpr int select_default_value(Box<T>) {
    return V == seed ? 12 : 14;
}
}

namespace PackOrderConstexprInlineEnumAdlTemplate {
using PackOrderConstexprInlineEnumAdl::Value;
using PackOrderConstexprInlineEnumAdl::inline_seed;

template<int ValueArgument>
constexpr int qualified_enumerator_value() {
    return ValueArgument;
}

template<Value V, typename T,
         template<typename, int = default_value(V)> class Box>
constexpr int select_default_value(Box<T>) {
    return V == inline_seed ? 12 : 14;
}
}

namespace PackOrderInlineEnumAdlAlias =
    PackOrderConstexprInlineEnumAdl::V1;

template<typename T>
struct PackOrderTemplateBox {};

template<typename T, typename U = void>
struct PackOrderDefaultedTemplateBox {};

template<typename T, typename U = T>
struct PackOrderDependentDefaultTemplateBox {};

template<typename T, int N = sizeof(T)>
struct PackOrderNonTypeDefaultTemplateBox {};

template<typename T, template<typename> class Box = PackOrderTemplateBox>
struct PackOrderTemplateDefaultTemplateBox {};

template<typename T, int N>
struct PackOrderOuterNonTypeDefaultTemplateBox {};

struct PackOrderNestedTypeDefaultValue {
    using value_type = unsigned;
    int value;
};

template<typename T, typename U = typename T::value_type>
struct PackOrderNestedTypeDefaultTemplateBox {};

struct PackOrderNestedTypeDefaultIntermediate {
    using nested_type = unsigned;
};

struct PackOrderNestedTypeDefaultDeepValue {
    using value_type = PackOrderNestedTypeDefaultIntermediate;
    int value;
};

template<typename T,
         typename U = typename T::value_type::nested_type>
struct PackOrderNestedTypeDefaultDeepTemplateBox {};

struct PackOrderConstrainedValue {
    int value;
};

template<PackOrderHasValue T>
int choose_constraint_partial_order(T) {
    return 149;
}

template<PackOrderHasValueAndSize T>
int choose_constraint_partial_order(T) {
    return 150;
}

template<template<typename> class Box, typename T>
    requires PackOrderHasValue<T>
int choose_template_template_constraint_order(Box<T>) {
    return 153;
}

template<template<typename> class Box, typename T>
    requires PackOrderHasValueAndSize<T>
int choose_template_template_constraint_order(Box<T>) {
    return 154;
}

template<template<PackOrderHasValue U> class Box, typename T>
    requires PackOrderHasValue<T>
int choose_constrained_template_template_signature_order(Box<T>) {
    return 157;
}

template<template<typename U> class Box, typename T>
    requires PackOrderHasValueAndSize<T>
int choose_constrained_template_template_signature_order(Box<T>) {
    return 158;
}

template<template<typename, typename = void> class Box, typename T>
    requires PackOrderHasValueAndSize<T>
int choose_defaulted_template_template_constraint_order(Box<T, void>) {
    return 155;
}

template<template<typename, typename> class Box, typename T>
    requires PackOrderHasValue<T>
int choose_defaulted_template_template_constraint_order(Box<T, void>) {
    return 156;
}

template<typename T, template<typename, typename = T> class Box>
    requires PackOrderHasValueAndSize<T>
int choose_dependent_default_template_template_constraint_order(
    Box<T>) {
    return 159;
}

template<typename T, template<typename, typename> class Box>
    requires PackOrderHasValue<T>
int choose_dependent_default_template_template_constraint_order(
    Box<T, T>) {
    return 160;
}

template<typename T, template<typename, int = sizeof(T)> class Box>
    requires PackOrderHasValueAndSize<T>
int choose_non_type_default_template_template_order(Box<T>) {
    return 161;
}

template<typename T, template<typename, int> class Box>
    requires PackOrderHasValue<T>
int choose_non_type_default_template_template_order(Box<T, sizeof(T)>) {
    return 162;
}

template<typename T,
         template<typename,
                  template<typename> class = PackOrderTemplateBox> class Box>
    requires PackOrderHasValueAndSize<T>
int choose_template_default_template_template_order(Box<T>) {
    return 163;
}

template<typename T,
         template<typename, template<typename> class> class Box>
    requires PackOrderHasValue<T>
int choose_template_default_template_template_order(
    Box<T, PackOrderTemplateBox>) {
    return 164;
}

template<int N, typename T, template<typename, int = N> class Box>
    requires PackOrderHasValueAndSize<T>
int choose_outer_non_type_default_template_template_order(Box<T>) {
    return 165;
}

template<int N, typename T, template<typename, int> class Box>
    requires PackOrderHasValue<T>
int choose_outer_non_type_default_template_template_order(Box<T, N>) {
    return 166;
}

template<int N, typename T, template<typename, int = N + 1> class Box>
    requires PackOrderHasValueAndSize<T>
int choose_outer_non_type_expression_default_template_template_order(
    Box<T>, T (&values)[N]) {
    return values[0].value == 3 ? 167 : 168;
}

template<int N, typename T, template<typename, int> class Box>
    requires PackOrderHasValue<T>
int choose_outer_non_type_expression_default_template_template_order(
    Box<T, N + 1>, T (&values)[N]) {
    return values[0].value == 3 ? 169 : 170;
}

template<int N, typename T, template<typename, int = 2 * N + 1> class Box>
    requires PackOrderHasValueAndSize<T>
int choose_outer_non_type_arithmetic_default_template_template_order(
    Box<T>, T (&values)[N]) {
    return values[0].value == 3 ? 171 : 172;
}

template<int N, typename T, template<typename, int> class Box>
    requires PackOrderHasValue<T>
int choose_outer_non_type_arithmetic_default_template_template_order(
    Box<T, 2 * N + 1>, T (&values)[N]) {
    return values[0].value == 3 ? 173 : 174;
}

template<int N, typename T,
         template<typename,
                  int = static_cast<int>(3 * N + 2)> class Box>
    requires PackOrderHasValueAndSize<T>
int choose_outer_non_type_cast_default_template_template_order(
    Box<T>, T (&values)[N]) {
    return values[0].value == 3 ? 183 : 184;
}

template<int N, typename T, template<typename, int> class Box>
    requires PackOrderHasValue<T>
int choose_outer_non_type_cast_default_template_template_order(
    Box<T, static_cast<int>(3 * N + 2)>, T (&values)[N]) {
    return values[0].value == 3 ? 185 : 186;
}

template<int N, typename T,
         template<typename,
                  int = (PackOrderPositiveNonType<N> ? 3 * N + 2
                                                     : 3 * N + 3)> class Box>
    requires PackOrderHasValueAndSize<T>
int choose_outer_non_type_concept_default_template_template_order(
    Box<T>, T (&values)[N]) {
    return values[0].value == 3 ? 187 : 188;
}

template<int N, typename T, template<typename, int> class Box>
    requires PackOrderHasValue<T>
int choose_outer_non_type_concept_default_template_template_order(
    Box<T, (PackOrderPositiveNonType<N> ? 3 * N + 2 : 3 * N + 3)>,
    T (&values)[N]) {
    return values[0].value == 3 ? 189 : 190;
}

template<int N, typename T,
         template<typename,
                  int = pack_order_constexpr_default_value(N)> class Box>
    requires PackOrderHasValueAndSize<T>
int choose_outer_non_type_constexpr_call_default_template_template_order(
    Box<T>, T (&values)[N]) {
    return values[0].value == 3 ? 191 : 192;
}

template<int N, typename T, template<typename, int> class Box>
    requires PackOrderHasValue<T>
int choose_outer_non_type_constexpr_call_default_template_template_order(
    Box<T, pack_order_constexpr_default_value(N)>, T (&values)[N]) {
    return values[0].value == 3 ? 193 : 194;
}

template<int N, typename T,
         template<typename,
                  int = pack_order_constexpr_template_default_value(N)> class Box>
    requires PackOrderHasValueAndSize<T>
int choose_outer_non_type_constexpr_template_call_default_template_template_order(
    Box<T>, T (&values)[N]) {
    return values[0].value == 3 ? 195 : 196;
}

template<int N, typename T, template<typename, int> class Box>
    requires PackOrderHasValue<T>
int choose_outer_non_type_constexpr_template_call_default_template_template_order(
    Box<T, pack_order_constexpr_template_default_value(N)>,
    T (&values)[N]) {
    return values[0].value == 3 ? 197 : 198;
}

template<int N, typename T,
         template<typename,
                  int = pack_order_overloaded_constexpr_template_default_value(N)> class Box>
    requires PackOrderHasValueAndSize<T>
int choose_outer_non_type_overloaded_constexpr_template_call_default_order(
    Box<T>, T (&values)[N]) {
    return values[0].value == 3 ? 199 : 200;
}

template<int N, typename T, template<typename, int> class Box>
    requires PackOrderHasValue<T>
int choose_outer_non_type_overloaded_constexpr_template_call_default_order(
    Box<T, pack_order_overloaded_constexpr_template_default_value(N)>,
    T (&values)[N]) {
    return values[0].value == 3 ? 201 : 202;
}

template<typename T,
         template<typename, typename = typename T::value_type> class Box>
    requires PackOrderHasValueAndSize<T>
int choose_nested_type_default_template_template_order(Box<int>, T value) {
    return value.value == 7 ? 175 : 176;
}

template<typename T, template<typename, typename> class Box>
    requires PackOrderHasValue<T>
int choose_nested_type_default_template_template_order(
    Box<int, typename T::value_type>, T value) {
    return value.value == 7 ? 177 : 178;
}

template<typename T,
         template<typename,
                  typename = typename T::value_type::nested_type> class Box>
    requires PackOrderHasValueAndSize<T>
int choose_nested_deep_type_default_template_template_order(Box<int>,
                                                             T value) {
    return value.value == 7 ? 179 : 180;
}

template<typename T, template<typename, typename> class Box>
    requires PackOrderHasValue<T>
int choose_nested_deep_type_default_template_template_order(
    Box<int, typename T::value_type::nested_type>, T value) {
    return value.value == 7 ? 181 : 182;
}

template<typename T>
    requires (PackOrderHasValue<T> || PackOrderHasPositiveSize<T>)
int choose_constraint_disjunction_partial_order(T) {
    return 151;
}

template<PackOrderHasValue T>
int choose_constraint_disjunction_partial_order(T) {
    return 152;
}

template<typename T, typename... Rest>
int choose_nonpack_over_empty_pack(T, Rest...) {
    return 150;
}

template<typename T>
int choose_nonpack_over_empty_pack(T) {
    return 160;
}

int verify_function_template_pointer_overload() {
    ConversionNoexceptIntPointerFunction selected =
        conversion_function_template_overload;
    ConversionNoexceptIntPointerFunction explicit_noexcept =
        conversion_explicit_noexcept_function_template;
    ConversionNoexceptIntPointerFunction dependent_noexcept =
        conversion_dependent_noexcept_function_template;
    ConversionThrowingIntPointerFunction dependent_throwing =
        conversion_dependent_throwing_function_template;
    ConversionNoexceptIntPointerFunction non_template_preferred =
        conversion_non_template_preferred;
    ConversionNoexceptIntPointerFunction namespaced =
        ConversionFunctionTemplateNamespace::select;
    ConversionIntPairFunction packed = conversion_function_template_pack;
    ConversionIntFunction fixed_over_pack = choose_fixed_over_pack;
    ConversionIntPairFunction fixed_prefix_over_pack =
        choose_fixed_prefix_over_pack;
    return selected(nullptr) == 2 && explicit_noexcept(nullptr) == 3 &&
           dependent_noexcept(nullptr) == 4 &&
           dependent_throwing(nullptr) == 5 &&
           non_template_preferred(nullptr) == 6 &&
           namespaced(nullptr) == 8 &&
           packed(1, 2) == 6 &&
           fixed_over_pack(1) == 110 && fixed_prefix_over_pack(1, 2) == 130;
}

int verify_noexcept_function_template_rank() {
    ConversionNoexceptIntPointerFunction nonthrowing =
        conversion_noexcept_template_rank;
    ConversionThrowingIntPointerFunction throwing =
        conversion_noexcept_template_rank;
    return nonthrowing(nullptr) == 92 && throwing(nullptr) == 91;
}

int verify_function_template_using_lookup() {
    ConversionNoexceptIntPointerFunction using_directive =
        ConversionFunctionTemplateUsingDirectiveNamespace::select_via_directive;
    ConversionNoexceptIntPointerFunction using_declaration =
        ConversionFunctionTemplateUsingDeclarationNamespace::select_via_declaration;
    return using_directive(nullptr) == 81 &&
           using_declaration(nullptr) == 82;
}

template<typename T>
T forward_template(T value) {
    return value;
}

template<typename T>
int choose_conversion_before_partial_order(T, int) {
    return 10;
}

template<typename T>
int choose_conversion_before_partial_order(T*, long) {
    return 20;
}

template<typename T>
int choose_cv_qualification(const T*) {
    return 30;
}

template<typename T>
int choose_cv_qualification(const volatile T*) {
    return 40;
}

template<typename T>
int choose_reference_binding(const T&) {
    return 51;
}

template<typename T>
int choose_reference_binding(const T&&) {
    return 52;
}

template<typename T>
int choose_reference_cv_category(T&) {
    return 171;
}

template<typename T>
int choose_reference_cv_category(const T&) {
    return 172;
}

int choose_reference_qualification(const int&) {
    return 231;
}

int choose_reference_qualification(const volatile int&) {
    return 232;
}

int choose_reference_pointer_qualification(const int* const&) {
    return 245;
}

int choose_reference_pointer_qualification(int* const&) {
    return 246;
}

template<typename T>
int choose_reference_qualification_template(T, const int&) {
    return 241;
}

template<typename T>
int choose_reference_qualification_template(T, const volatile int&) {
    return 242;
}

template<typename T>
int choose_reference_pointer_qualification_template(
    T, const int* const&) {
    return 243;
}

template<typename T>
int choose_reference_pointer_qualification_template(T, int* const&) {
    return 244;
}

template<typename T>
int choose_rvalue_cv_category(T&&) {
    return 173;
}

template<typename T>
int choose_rvalue_cv_category(const T&&) {
    return 174;
}

template<typename T>
int choose_pointer_bool(T*, bool) {
    return 61;
}

template<typename T>
int choose_pointer_bool(T*, void*) {
    return 62;
}

template<typename T>
int choose_pointer_subsequence(T*, void*) {
    return 71;
}

template<typename T>
int choose_pointer_subsequence(T*, const void*) {
    return 72;
}

struct ConversionPointerTrailingSequence {
    int* value;
    operator int*() const { return value; }
};

int choose_same_conversion_trailing_sequence(int*) {
    return 241;
}

int choose_same_conversion_trailing_sequence(const int*) {
    return 242;
}

template<typename Tag>
int choose_same_conversion_trailing_sequence_template(Tag, int*) {
    return 243;
}

template<typename Tag>
int choose_same_conversion_trailing_sequence_template(Tag, const int*) {
    return 244;
}

int choose_pointer_subsequence_non_template(void*) {
    return 73;
}

int choose_pointer_subsequence_non_template(const void*) {
    return 74;
}

int choose_nested_pointer_qualification(int* const*) {
    return 75;
}

int choose_nested_pointer_qualification(const int* const*) {
    return 76;
}

int choose_array_bound_reference(int (&value)[]) {
    return value[0] + 10;
}

int choose_array_bound_reference(int (&value)[1]) {
    return value[0] + 20;
}

template<typename Element>
int choose_array_bound_reference_template(Element (&value)[]) {
    return value[0] + 30;
}

template<typename Element>
int choose_array_bound_reference_template(Element (&value)[1]) {
    return value[0] + 40;
}

int choose_array_bound_reference_cv(int (&value)[]) {
    return value[0] + 50;
}

int choose_array_bound_reference_cv(const int (&value)[1]) {
    return value[0] + 60;
}

template<typename Element>
int choose_array_bound_reference_cv_template(Element (&value)[]) {
    return value[0] + 70;
}

template<typename Element>
int choose_array_bound_reference_cv_template(const Element (&value)[1]) {
    return value[0] + 80;
}

template<typename Element, int Extent>
int choose_template_array_extent(Element (&value)[Extent]) {
    return value[0] + 160;
}

template<typename Element>
int choose_template_array_extent(Element (&value)[1]) {
    return value[0] + 170;
}

template<typename Element, int Extent>
int choose_template_array_pointer_extent(Element (*value)[Extent]) {
    return (*value)[0] + 180;
}

template<typename Element>
int choose_template_array_pointer_extent(Element (*value)[1]) {
    return (*value)[0] + 190;
}

template<typename Element, int Extent>
int choose_template_nested_array_pointer_extent(
    Element (*value)[2][Extent]) {
    return (*value)[0][0] + 200;
}

template<typename Element>
int choose_template_nested_array_pointer_extent(Element (*value)[2][1]) {
    return (*value)[0][0] + 210;
}

template<typename T>
int choose_nearer_base(T*, ConversionBaseRoot*) {
    return 81;
}

template<typename T>
int choose_nearer_base(T*, ConversionBaseMiddle*) {
    return 82;
}

template<typename T>
int choose_nearer_base_reference(T*, ConversionBaseRoot&) {
    return 85;
}

template<typename T>
int choose_nearer_base_reference(T*, ConversionBaseMiddle&) {
    return 86;
}

int choose_cv_nearer_base_reference(const ConversionBaseRoot&) {
    return 102;
}

int choose_cv_nearer_base_reference(ConversionBaseMiddle&) {
    return 103;
}

template<typename T>
int choose_cv_nearer_base_reference_template(
    T*, const ConversionBaseRoot&) {
    return 104;
}

template<typename T>
int choose_cv_nearer_base_reference_template(
    T*, ConversionBaseMiddle&) {
    return 105;
}

int choose_cv_derived_base_reference(ConversionBaseRoot&) {
    return 106;
}

int choose_cv_derived_base_reference(const ConversionBaseMiddle&) {
    return 107;
}

template<typename T>
int choose_cv_derived_base_reference_template(
    T*, ConversionBaseRoot&) {
    return 108;
}

template<typename T>
int choose_cv_derived_base_reference_template(
    T*, const ConversionBaseMiddle&) {
    return 109;
}

int choose_nearer_base_reference_non_template(ConversionBaseRoot&) {
    return 87;
}

int choose_nearer_base_reference_non_template(ConversionBaseMiddle&) {
    return 88;
}

int choose_nearer_base_value_non_template(ConversionBaseRoot) {
    return 91;
}

int choose_nearer_base_value_non_template(ConversionBaseMiddle) {
    return 92;
}

template<typename T>
int choose_nearer_base_value_template(T*, ConversionBaseRoot) {
    return 93;
}

template<typename T>
int choose_nearer_base_value_template(T*, ConversionBaseMiddle) {
    return 94;
}

int choose_nearer_base_pointer_non_template(ConversionBaseRoot*) {
    return 89;
}

int choose_nearer_base_pointer_non_template(ConversionBaseMiddle*) {
    return 90;
}

int choose_derived_base_qualification(ConversionBaseRoot*) {
    return 99;
}

int choose_derived_base_qualification(const ConversionBaseRoot*) {
    return 100;
}

int choose_cv_nearer_base_pointer(const ConversionBaseRoot*) {
    return 111;
}

int choose_cv_nearer_base_pointer(ConversionBaseMiddle*) {
    return 112;
}

int choose_cv_incomparable_nearer_base_pointer(
    const ConversionBaseMiddle*) {
    return 115;
}

int choose_cv_incomparable_nearer_base_pointer(
    volatile ConversionBaseRoot*) {
    return 116;
}

template<typename T>
int choose_derived_base_qualification_template(
    T*, ConversionBaseRoot*) {
    return 101;
}

template<typename T>
int choose_derived_base_qualification_template(
    T*, const ConversionBaseRoot*) {
    return 102;
}

template<typename T>
int choose_cv_nearer_base_pointer_template(
    T*, const ConversionBaseRoot*) {
    return 113;
}

template<typename T>
int choose_cv_nearer_base_pointer_template(
    T*, ConversionBaseMiddle*) {
    return 114;
}

template<typename T>
int choose_cv_incomparable_nearer_base_pointer_template(
    T*, const ConversionBaseMiddle*) {
    return 117;
}

template<typename T>
int choose_cv_incomparable_nearer_base_pointer_template(
    T*, volatile ConversionBaseRoot*) {
    return 118;
}

int choose_fixed_enum_promotion(unsigned char) {
    return 211;
}

int choose_fixed_enum_promotion(int) {
    return 212;
}

int choose_pointer_to_bool_non_template(bool) {
    return 215;
}

int choose_pointer_to_bool_non_template(void*) {
    return 216;
}

int choose_array_to_bool(bool value) {
    return value ? 245 : 246;
}

int choose_function_to_bool(bool value) {
    return value ? 247 : 248;
}

int choose_array_to_bool_reference(const bool& value) {
    return value ? 249 : 250;
}

int choose_function_to_bool_reference(const bool& value) {
    return value ? 251 : 252;
}

int choose_array_to_bool_rvalue_reference(bool&& value) {
    return value ? 253 : 254;
}

int choose_function_to_bool_rvalue_reference(bool&& value) {
    return value ? 255 : 256;
}

int choose_pointer_to_bool_reference(const bool& value) {
    return value ? 257 : 258;
}

int choose_pointer_to_bool_rvalue_reference(bool&& value) {
    return value ? 259 : 260;
}

int choose_member_pointer_to_bool(bool value) {
    return value ? 261 : 262;
}

int choose_member_pointer_to_bool_reference(const bool& value) {
    return value ? 263 : 264;
}

int choose_member_pointer_to_bool_rvalue_reference(bool&& value) {
    return value ? 265 : 266;
}

static bool convert_data_member_pointer_to_bool(
    ConversionDataMemberBoolPointer pointer) {
    bool result = pointer;
    return result;
}

static bool convert_function_member_pointer_to_bool(
    ConversionFunctionMemberBoolPointer pointer) {
    return pointer;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
static int verify_array_to_bool_decay() {
    int value[1] = { 1 };
    int pointer_result = choose_pointer_to_bool_non_template(value);
    return pointer_result == 216 ? choose_array_to_bool(value)
                                 : pointer_result;
}

static int verify_array_to_bool_reference_decay() {
    int value[1] = { 1 };
    return choose_array_to_bool_reference(value);
}

static int verify_function_to_bool_decay() {
    return choose_function_to_bool(conversion_noexcept_function);
}

static int verify_function_to_bool_reference_decay() {
    return choose_function_to_bool_reference(
        conversion_noexcept_function);
}

static int verify_array_to_bool_rvalue_reference_decay() {
    int value[1] = { 1 };
    return choose_array_to_bool_rvalue_reference(value);
}

static int verify_function_to_bool_rvalue_reference_decay() {
    return choose_function_to_bool_rvalue_reference(
        conversion_noexcept_function);
}

static int verify_object_pointer_to_bool_reference() {
    int value = 1;
    int* pointer = &value;
    return choose_pointer_to_bool_reference(pointer);
}

static int verify_function_pointer_to_bool_reference() {
    ConversionNoexceptFunctionPointer pointer =
        conversion_noexcept_function;
    return choose_pointer_to_bool_reference(pointer);
}

static int verify_object_pointer_to_bool_rvalue_reference() {
    int value = 1;
    int* pointer = &value;
    return choose_pointer_to_bool_rvalue_reference(pointer);
}

static int verify_function_pointer_to_bool_rvalue_reference() {
    ConversionNoexceptFunctionPointer pointer =
        conversion_noexcept_function;
    return choose_pointer_to_bool_rvalue_reference(pointer);
}

static bool verify_data_member_pointer_to_bool() {
    ConversionDataMemberBoolPointer value =
        &ConversionMemberPointerBoolOwner::value;
    ConversionDataMemberBoolPointer null_value = nullptr;
    return choose_member_pointer_to_bool(value) == 261 &&
           choose_member_pointer_to_bool(null_value) == 262 &&
           choose_member_pointer_to_bool_reference(value) == 263 &&
           choose_member_pointer_to_bool_reference(null_value) == 264 &&
           choose_member_pointer_to_bool_rvalue_reference(value) == 265 &&
           choose_member_pointer_to_bool_rvalue_reference(null_value) == 266 &&
           convert_data_member_pointer_to_bool(value) &&
           !convert_data_member_pointer_to_bool(null_value) &&
           static_cast<bool>(value) && !static_cast<bool>(null_value);
}

static bool verify_function_member_pointer_to_bool() {
    ConversionFunctionMemberBoolPointer value =
        &ConversionMemberPointerBoolOwner::is_positive;
    ConversionFunctionMemberBoolPointer null_value = nullptr;
    return choose_member_pointer_to_bool(value) == 261 &&
           choose_member_pointer_to_bool(null_value) == 262 &&
           choose_member_pointer_to_bool_reference(value) == 263 &&
           choose_member_pointer_to_bool_reference(null_value) == 264 &&
           choose_member_pointer_to_bool_rvalue_reference(value) == 265 &&
           choose_member_pointer_to_bool_rvalue_reference(null_value) == 266 &&
           convert_function_member_pointer_to_bool(value) &&
           !convert_function_member_pointer_to_bool(null_value) &&
           static_cast<bool>(value) && !static_cast<bool>(null_value);
}

static int contextual_data_member_pointer_bool(
    ConversionDataMemberBoolPointer pointer) {
    if (pointer) {
        return pointer && (pointer ? 271 : 272) == 271 ? 271 : 272;
    }
    return !pointer ? 273 : 274;
}

static int contextual_function_member_pointer_bool(
    ConversionFunctionMemberBoolPointer pointer) {
    if (pointer) {
        return pointer && (pointer ? 275 : 276) == 275 ? 275 : 276;
    }
    return !pointer ? 277 : 278;
}

static bool verify_member_pointer_contextual_bool() {
    ConversionDataMemberBoolPointer data_value =
        &ConversionMemberPointerBoolOwner::value;
    ConversionDataMemberBoolPointer data_null = nullptr;
    ConversionFunctionMemberBoolPointer function_value =
        &ConversionMemberPointerBoolOwner::is_positive;
    ConversionFunctionMemberBoolPointer function_null = nullptr;
    return contextual_data_member_pointer_bool(data_value) == 271 &&
           contextual_data_member_pointer_bool(data_null) == 273 &&
           contextual_function_member_pointer_bool(function_value) == 275 &&
           contextual_function_member_pointer_bool(function_null) == 277;
}
#pragma GCC diagnostic pop

int choose_nullptr_to_bool_non_template(bool) {
    return 217;
}

int choose_nullptr_to_bool_non_template(void*) {
    return 218;
}

template<typename T>
int choose_nullptr_to_bool_template(T, bool) {
    return 219;
}

template<typename T>
int choose_nullptr_to_bool_template(T, void*) {
    return 220;
}

int choose_noexcept_function_pointer(
    ConversionThrowingFunctionPointer) {
    return 221;
}

int choose_noexcept_function_pointer(
    ConversionNoexceptFunctionPointer) {
    return 222;
}

template<typename T>
int choose_noexcept_function_pointer_template(
    T, ConversionThrowingFunctionPointer) {
    return 223;
}

template<typename T>
int choose_noexcept_function_pointer_template(
    T, ConversionNoexceptFunctionPointer) {
    return 224;
}

namespace ConversionFunctionOverloadNamespace {
void conversion_overloaded_noexcept_function(int) {}
void conversion_overloaded_noexcept_function() noexcept {}

int verify_function_pointer_overload_context() {
    ConversionNoexceptFunctionPointer selected =
        conversion_overloaded_noexcept_function;
    ConversionNoexceptFunctionPointer global =
        ::conversion_overloaded_noexcept_function;
    return selected != global &&
           ::choose_noexcept_function_pointer(
               conversion_overloaded_noexcept_function) == 222 &&
           ::choose_noexcept_function_pointer_template(
               1, conversion_overloaded_noexcept_function) == 224;
}
}

namespace ConversionFunctionImportedNamespace {
void imported_overloaded_function(int) {}
void imported_overloaded_function() noexcept {}
}

namespace ConversionFunctionUsingDeclarationNamespace {
using ConversionFunctionImportedNamespace::imported_overloaded_function;

int verify_function_pointer_overload_context() {
    ConversionNoexceptFunctionPointer selected =
        imported_overloaded_function;
    ConversionNoexceptFunctionPointer qualified =
        ::ConversionFunctionImportedNamespace::imported_overloaded_function;
    return selected == qualified &&
           ::choose_noexcept_function_pointer(
               imported_overloaded_function) == 222 &&
           ::choose_noexcept_function_pointer_template(
               1, imported_overloaded_function) == 224;
}
}

namespace ConversionFunctionUsingDirectiveNamespace {
using namespace ConversionFunctionImportedNamespace;

int verify_function_pointer_overload_context() {
    ConversionNoexceptFunctionPointer selected =
        imported_overloaded_function;
    ConversionNoexceptFunctionPointer qualified =
        ::ConversionFunctionImportedNamespace::imported_overloaded_function;
    return selected == qualified &&
           ::choose_noexcept_function_pointer(
               imported_overloaded_function) == 222 &&
           ::choose_noexcept_function_pointer_template(
               1, imported_overloaded_function) == 224;
}
}

template<typename T>
int choose_fixed_enum_template(T, unsigned char) {
    return 213;
}

template<typename T>
int choose_fixed_enum_template(T, int) {
    return 214;
}

template<typename T>
int choose_nearer_member_pointer(
    T, int ConversionMemberPointerMiddle::*) {
    return 191;
}

template<typename T>
int choose_nearer_member_pointer(
    T, int ConversionMemberPointerLeaf::*) {
    return 192;
}

int choose_nearer_member_pointer_non_template(
    int ConversionMemberPointerMiddle::*) {
    return 193;
}

int choose_nearer_member_pointer_non_template(
    int ConversionMemberPointerLeaf::*) {
    return 194;
}

template<typename T>
int choose_nearer_member_function_pointer(
    T, int (ConversionMemberPointerMiddle::*)(int) const) {
    return 195;
}

template<typename T>
int choose_nearer_member_function_pointer(
    T, int (ConversionMemberPointerLeaf::*)(int) const) {
    return 196;
}

int choose_nearer_member_function_pointer_non_template(
    int (ConversionMemberPointerMiddle::*)(int) const) {
    return 197;
}

int choose_nearer_member_function_pointer_non_template(
    int (ConversionMemberPointerLeaf::*)(int) const) {
    return 198;
}

template<typename T>
int choose_member_pointer_member_type_cv(
    T, int ConversionMemberPointerMiddle::*) {
    return 201;
}

template<typename T>
int choose_member_pointer_member_type_cv(
    T, const int ConversionMemberPointerMiddle::*) {
    return 202;
}

int choose_member_pointer_member_type_cv_non_template(
    int ConversionMemberPointerMiddle::*) {
    return 203;
}

int choose_member_pointer_member_type_cv_non_template(
    const int ConversionMemberPointerMiddle::*) {
    return 204;
}

template<typename T>
int choose_member_pointer_owner_cv_subsequence(
    T, int ConversionMemberPointerMiddle::*) {
    return 233;
}

template<typename T>
int choose_member_pointer_owner_cv_subsequence(
    T, const int ConversionMemberPointerMiddle::*) {
    return 234;
}

int choose_member_pointer_owner_cv_subsequence_non_template(
    int ConversionMemberPointerMiddle::*) {
    return 235;
}

int choose_member_pointer_owner_cv_subsequence_non_template(
    const int ConversionMemberPointerMiddle::*) {
    return 236;
}

template<typename T>
int choose_nested_member_pointer_qualification(
    T, int* ConversionNestedMemberPointerOwner::*) {
    return 245;
}

template<typename T>
int choose_nested_member_pointer_qualification(
    T, const int* ConversionNestedMemberPointerOwner::*) {
    return 246;
}

int choose_nested_member_pointer_qualification_non_template(
    int* ConversionNestedMemberPointerOwner::*) {
    return 247;
}

int choose_nested_member_pointer_qualification_non_template(
    const int* ConversionNestedMemberPointerOwner::*) {
    return 248;
}

template<typename T>
int choose_member_function_pointer_noexcept(
    T, void (ConversionMemberPointerMiddle::*)() noexcept) {
    return 205;
}

template<typename T>
int choose_member_function_pointer_noexcept(
    T, void (ConversionMemberPointerMiddle::*)()) {
    return 206;
}

int choose_member_function_pointer_noexcept_non_template(
    void (ConversionMemberPointerMiddle::*)() noexcept) {
    return 207;
}

int choose_member_function_pointer_noexcept_non_template(
    void (ConversionMemberPointerMiddle::*)()) {
    return 208;
}

template<typename T>
int choose_member_function_pointer_owner_noexcept(
    T, void (ConversionMemberPointerMiddle::*)() noexcept) {
    return 219;
}

template<typename T>
int choose_member_function_pointer_owner_noexcept(
    T, void (ConversionMemberPointerLeaf::*)()) {
    return 220;
}

int choose_member_function_pointer_owner_noexcept_non_template(
    void (ConversionMemberPointerMiddle::*)() noexcept) {
    return 221;
}

int choose_member_function_pointer_owner_noexcept_non_template(
    void (ConversionMemberPointerLeaf::*)()) {
    return 222;
}

template<typename T>
int choose_member_function_pointer_owner_noexcept_tradeoff(
    T, void (ConversionMemberPointerMiddle::*)()) {
    return 223;
}

template<typename T>
int choose_member_function_pointer_owner_noexcept_tradeoff(
    T, void (ConversionMemberPointerLeaf::*)() noexcept) {
    return 224;
}

int choose_member_function_pointer_owner_noexcept_tradeoff_non_template(
    void (ConversionMemberPointerMiddle::*)()) {
    return 225;
}

int choose_member_function_pointer_owner_noexcept_tradeoff_non_template(
    void (ConversionMemberPointerLeaf::*)() noexcept) {
    return 226;
}

template<typename T>
int choose_member_pointer_owner_cv_tradeoff(
    T, const int ConversionMemberPointerMiddle::*) {
    return 227;
}

template<typename T>
int choose_member_pointer_owner_cv_tradeoff(
    T, volatile int ConversionMemberPointerLeaf::*) {
    return 228;
}

int choose_member_pointer_owner_cv_tradeoff_non_template(
    const int ConversionMemberPointerMiddle::*) {
    return 229;
}

int choose_member_pointer_owner_cv_tradeoff_non_template(
    volatile int ConversionMemberPointerLeaf::*) {
    return 230;
}

template<typename T>
int choose_member_pointer_owner_cv_reverse_tradeoff(
    T, volatile int ConversionMemberPointerMiddle::*) {
    return 231;
}

template<typename T>
int choose_member_pointer_owner_cv_reverse_tradeoff(
    T, const int ConversionMemberPointerLeaf::*) {
    return 232;
}

int choose_member_pointer_owner_cv_reverse_tradeoff_non_template(
    volatile int ConversionMemberPointerMiddle::*) {
    return 233;
}

int choose_member_pointer_owner_cv_reverse_tradeoff_non_template(
    const int ConversionMemberPointerLeaf::*) {
    return 234;
}

template<typename T>
int choose_member_pointer_owner_qualification_tradeoff(
    T, int ConversionMemberPointerMiddle::*) {
    return 235;
}

template<typename T>
int choose_member_pointer_owner_qualification_tradeoff(
    T, const int ConversionMemberPointerLeaf::*) {
    return 236;
}

int choose_member_pointer_owner_qualification_tradeoff_non_template(
    int ConversionMemberPointerMiddle::*) {
    return 237;
}

int choose_member_pointer_owner_qualification_tradeoff_non_template(
    const int ConversionMemberPointerLeaf::*) {
    return 238;
}

template<typename T>
int choose_member_pointer_owner_qualification_reverse_tradeoff(
    T, const int ConversionMemberPointerMiddle::*) {
    return 239;
}

template<typename T>
int choose_member_pointer_owner_qualification_reverse_tradeoff(
    T, int ConversionMemberPointerLeaf::*) {
    return 240;
}

int choose_member_pointer_owner_qualification_reverse_tradeoff_non_template(
    const int ConversionMemberPointerMiddle::*) {
    return 241;
}

int choose_member_pointer_owner_qualification_reverse_tradeoff_non_template(
    int ConversionMemberPointerLeaf::*) {
    return 242;
}

template<typename T>
int choose_base_over_void(T*, ConversionBaseRoot*) {
    return 83;
}

template<typename T>
int choose_base_over_void(T*, void*) {
    return 84;
}

int choose_base_over_void_non_template(ConversionBaseRoot*) {
    return 97;
}

int choose_base_over_void_non_template(void*) {
    return 98;
}

template<typename T>
int choose_nearer_base_value(T*, ConversionValueRoot value) {
    return value.root_value == 41 ? 91 : -91;
}

template<typename T>
int choose_nearer_base_value(T*, ConversionValueMiddle value) {
    return value.prefix_value == 37 && value.root_value == 41 &&
                   value.middle_value == 43
               ? 92
               : -92;
}

int choose_nearer_base_value_non_template(ConversionValueRoot value) {
    return value.root_value == 41 ? 93 : -93;
}

int choose_nearer_base_value_non_template(ConversionValueMiddle value) {
    return value.prefix_value == 37 && value.root_value == 41 &&
                   value.middle_value == 43
               ? 94
               : -94;
}

static ConversionValueLeaf make_conversion_value_leaf(void) {
    ConversionValueLeaf value;
    value.prefix_value = 37;
    value.root_value = 41;
    value.middle_value = 43;
    value.leaf_value = 47;
    return value;
}

static int consume_conversion_value_virtual_root(
    ConversionValueVirtualRoot value) noexcept {
    return value.root_value;
}

static ConversionValueCleanupLeaf make_conversion_value_cleanup_leaf(
    void) noexcept {
    return ConversionValueCleanupLeaf{{49}, {53}, 59};
}

static int consume_conversion_value_cleanup_root(
    ConversionValueCleanupRoot value) noexcept {
    return value.root_value;
}

static ConversionValueVirtualCleanupLeaf
make_conversion_value_virtual_cleanup_leaf(void) noexcept {
    ConversionValueVirtualCleanupLeaf value;
    value.root_value = 73;
    value.middle_value = 79;
    value.leaf_value = 83;
    return value;
}

static int consume_conversion_value_virtual_cleanup_root(
    ConversionValueVirtualCleanupRoot value) noexcept {
    return value.root_value;
}

int main(void) {
    if (choose_non_type_class_pack_partial_order(
            FunctionTemplateNonTypePackBox<>{}) != 182) return 201;
    if (choose_non_type_class_pack_partial_order(
            FunctionTemplateNonTypePackBox<1>{}) != 183) return 202;
    if (choose_non_type_class_pack_partial_order(
            FunctionTemplateNonTypePackBox<1, 2>{}) != 183) return 203;
    if (choose_class_template_template_pack_expansion_order(
            FunctionTemplateTemplatePackBox<>{}) != 276) return 204;
    if (choose_class_template_template_pack_expansion_order(
            FunctionTemplateTemplatePackBox<
                FunctionTemplateTemplatePackItem>{}) != 277) return 205;
    if (choose_class_template_template_pack_expansion_order(
            FunctionTemplateTemplatePackBox<
                FunctionTemplateTemplatePackItem,
                FunctionTemplateTemplatePackFixed>{}) != 277) return 206;
    CxxFunctionTemplateAdl::Payload adl_payload{2};
    if (select_better_template(&adl_payload, 5) != 607) {
        return 164;
    }
    AdlTiePayload adl_tie_payload{2};
    if (adl_tie(&adl_tie_payload, 5) != 707) {
        return 165;
    }
    if (adl_tie<int>(&adl_tie_payload, 5) != 807) {
        return 166;
    }
    if (select_inline_namespace_adl(
            InlineNamespaceAssociatedAdl::Token{}, 5) != 345) {
        return 167;
    }
    if (cxx_free_variadic(7, 8, 9) != 7) {
        return 152;
    }
    if (cxx_free_variadic_template(7, 8, 9) != 8) {
        return 153;
    }
    if (cxx_free_ellipsis_only_template<unsigned char>() !=
        (int)sizeof(unsigned char)) {
        return 171;
    }
    if (cxx_free_ellipsis_only_template<unsigned long>(7, 8, 9) !=
        (int)sizeof(unsigned long)) {
        return 172;
    }
    if (cxx_free_variadic_overload(7, 8, 2.5f) != 107) {
        return 154;
    }
    char variadic_template_exact_match = 5;
    if (cxx_free_variadic_overload(
            variadic_template_exact_match, 2.5f) != 205) {
        return 155;
    }
    if (cxx_free_ellipsis_conversion_overload(
            CxxEllipsisImplicitInt{7}) != 3007) {
        return 167;
    }
    if (cxx_free_ellipsis_explicit_conversion_overload(
            CxxEllipsisExplicitInt{7}) != 5000) {
        return 168;
    }
    CxxMemberEllipsisProbe member_ellipsis_probe;
    if (member_ellipsis_probe.select(CxxEllipsisImplicitInt{7}) != 6007) {
        return 169;
    }
    if (member_ellipsis_probe.select(CxxEllipsisExplicitInt{7}) != 7000) {
        return 170;
    }
    if (cxx_free_fixed_overload(7) != 307) {
        return 156;
    }
    char fixed_template_exact_match = 5;
    if (cxx_free_fixed_overload(fixed_template_exact_match) != 405) {
        return 157;
    }
    if (cxx_free_fixed_multi_overload(7) != 607) {
        return 160;
    }
    char multi_template_exact_match = 5;
    if (cxx_free_fixed_multi_overload(multi_template_exact_match) != 705) {
        return 161;
    }
    CxxMixedOverloadDerived incomparable_overload_object;
    if (cxx_free_incomparable_overload(&incomparable_overload_object) != 3) {
        return 162;
    }
    CxxVariadicAggregatePayload variadic_payload = { 11, 3.5 };
    if (cxx_free_variadic_payload_overload(2, variadic_payload) != 1016) {
        return 158;
    }
    char aggregate_template_exact_match = 5;
    if (cxx_free_variadic_payload_overload(
            aggregate_template_exact_match, variadic_payload) != 2019) {
        return 159;
    }
    int value = 5;
    int* pointer = &value;
    PackOrderConstrainedValue constrained_value = { value };
    PackOrderConstrainedValue constrained_values[4] = {
        { 3 }, { 0 }, { 0 }, { 0 }
    };
    PackOrderNestedTypeDefaultValue nested_type_default_value = { 7 };
    PackOrderNestedTypeDefaultDeepValue nested_deep_type_default_value = {
        7
    };
    ConversionPointerTrailingSequence conversion_pointer{&value};
    int** pointer_to_pointer = &pointer;
    int array_value[1] = { value };
    int (*array_pointer)[1] = &array_value;
    int matrix_value[2][1] = { { value }, { value + 1 } };
    int (*matrix_pointer)[2][1] = &matrix_value;
    ConversionNoexceptFunctionPointer noexcept_pointer =
        &conversion_noexcept_function;
    ConversionExplicitThrowingFunctionPointer explicit_throwing_pointer =
        &conversion_explicit_throwing_function;
    ConversionExplicitNoexceptFunctionPointer explicit_noexcept_pointer =
        &conversion_explicit_noexcept_function;
    int (ConversionMemberPointerRoot::*root_member_function)(int) const =
        &ConversionMemberPointerRoot::function;
    void (ConversionMemberPointerRoot::*root_noexcept_member_function)()
        noexcept = &ConversionMemberPointerRoot::noexcept_function;
    void (ConversionMemberPointerRoot::*root_throwing_member_function)() =
        &ConversionMemberPointerRoot::throwing_function;
    const int const_value = 6;
    ConversionBaseLeaf leaf;
    ConversionBaseLeaf* leaf_pointer = &leaf;
    int class_value_rank_probe = 0;
    if (choose_nearer_base_value_non_template(leaf) != 92) return 207;
    if (choose_nearer_base_value_template(&class_value_rank_probe, leaf) !=
        94) return 208;
    ConversionValueLeaf value_leaf;
    ConversionValueLeaf* value_leaf_pointer = &value_leaf;
    ConversionValueVirtualLeaf virtual_value_leaf;
    value_leaf.prefix_value = 37;
    value_leaf.root_value = 41;
    value_leaf.middle_value = 43;
    value_leaf.leaf_value = 47;
    virtual_value_leaf.root_value = 61;
    virtual_value_leaf.middle_value = 67;
    virtual_value_leaf.leaf_value = 71;
    ConversionValueRoot root_copy = value_leaf;
    int cleanup_count_before = conversion_value_cleanup_count;
    int cleanup_root_value = consume_conversion_value_cleanup_root(
        make_conversion_value_cleanup_leaf());
    int virtual_cleanup_count_before = conversion_value_cleanup_count;
    int virtual_cleanup_root_value =
        consume_conversion_value_virtual_cleanup_root(
            make_conversion_value_virtual_cleanup_leaf());
    if (consume_conversion_value_virtual_root(virtual_value_leaf) != 61) {
        return 21;
    }
    if (virtual_cleanup_root_value != 73) return 22;
    if (conversion_value_cleanup_count !=
        virtual_cleanup_count_before + 2) {
        return 23;
    }
    if (choose_member_value_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberValueOne>{}) != 236) {
        return 81;
    }
    if (choose_member_value_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                2, FunctionTemplateMemberValueTwo>{}) != 235) {
        return 82;
    }
    if (choose_member_call_value_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallOne, int>{}) != 244) {
        return 90;
    }
    if (choose_member_call_value_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                2, FunctionTemplateMemberCallTwo, short, char>{}) != 243) {
        return 91;
    }
    if (choose_member_call_value_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallNonConstexpr>{}) != 244) {
        return 92;
    }
    if (choose_member_call_value_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallPrivate>{}) != 244) {
        return 93;
    }
    if (choose_member_call_argument_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallArgumentOne, int>{}) != 246) {
        return 94;
    }
    if (choose_member_call_argument_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                3, FunctionTemplateMemberCallArgumentTwo, short, char>{}) !=
        245) {
        return 95;
    }
    if (choose_member_call_argument_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallArgumentNonConstexpr, int>{}) !=
        246) {
        return 96;
    }
    if (choose_member_call_argument_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallArgumentPrivate, int>{}) != 246) {
        return 97;
    }
    if (choose_member_call_dependent_argument_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallArgumentTwo,
                FunctionTemplateMemberCallArgumentTag<0>, int>{}) != 248) {
        return 98;
    }
    if (choose_member_call_dependent_argument_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                4, FunctionTemplateMemberCallArgumentTwo,
                FunctionTemplateMemberCallArgumentTag<3>, short, char>{}) !=
        247) {
        return 99;
    }
    if (choose_member_call_nontype_argument_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallArgumentTwo,
                FunctionTemplateValuePackTag<0>, int>{}) != 250) {
        return 100;
    }
    if (choose_member_call_nontype_argument_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                4, FunctionTemplateMemberCallArgumentTwo,
                FunctionTemplateValuePackTag<3>, short, char>{}) != 249) {
        return 101;
    }
    if (choose_member_call_multiple_nontype_arguments_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallArgumentPair,
                FunctionTemplateValuePackTag<0>,
                FunctionTemplateValuePackTag<1>, int>{}) != 252) {
        return 102;
    }
    if (choose_member_call_multiple_nontype_arguments_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                23, FunctionTemplateMemberCallArgumentPair,
                FunctionTemplateValuePackTag<2>,
                FunctionTemplateValuePackTag<3>, short, char>{}) != 251) {
        return 103;
    }
    if (choose_member_call_overloaded_nontype_argument_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallOverloaded,
                FunctionTemplateValuePackTag<0>, int>{}) != 254) {
        return 104;
    }
    if (choose_member_call_overloaded_nontype_argument_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                4, FunctionTemplateMemberCallOverloaded,
                FunctionTemplateValuePackTag<3>, short>{}) != 253) {
        return 105;
    }
    if (choose_member_call_defaulted_nontype_argument_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallDefaulted,
                FunctionTemplateShortValuePackTag<0>, int>{}) != 256) {
        return 106;
    }
    if (choose_member_call_defaulted_nontype_argument_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                4, FunctionTemplateMemberCallDefaulted,
                FunctionTemplateShortValuePackTag<3>, short>{}) != 255) {
        return 107;
    }
    if (choose_member_call_no_explicit_arguments_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                25, FunctionTemplateMemberCallNoExplicitArguments, int>{}) !=
        257) {
        return 108;
    }
    if (choose_member_call_no_explicit_arguments_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallNoExplicitArgumentsOne,
                short>{}) != 257) {
        return 109;
    }
    if (choose_member_call_const_reference_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallConstReference,
                FunctionTemplateValuePackTag<0>, int>{}) != 260) {
        return 110;
    }
    if (choose_member_call_const_reference_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                4, FunctionTemplateMemberCallConstReference,
                FunctionTemplateValuePackTag<3>, short>{}) != 259) {
        return 111;
    }
    if (choose_member_call_rvalue_reference_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallRvalueReference,
                FunctionTemplateValuePackTag<0>, int>{}) != 262) {
        return 112;
    }
    if (choose_member_call_rvalue_reference_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                4, FunctionTemplateMemberCallRvalueReference,
                FunctionTemplateValuePackTag<3>, short>{}) != 261) {
        return 113;
    }
    if (choose_member_call_template_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallTemplate,
                FunctionTemplateValuePackTag<0>, int>{}) != 264) {
        return 114;
    }
    if (choose_member_call_template_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                4, FunctionTemplateMemberCallTemplate,
                FunctionTemplateValuePackTag<3>, short>{}) != 263) {
        return 115;
    }
    if (choose_member_call_template_explicit_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallTemplate,
                FunctionTemplateValuePackTag<0>, int>{}) != 266) {
        return 116;
    }
    if (choose_member_call_template_explicit_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                4, FunctionTemplateMemberCallTemplate,
                FunctionTemplateValuePackTag<3>, short>{}) != 265) {
        return 117;
    }
    if (choose_member_call_template_double_explicit_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallTemplate,
                FunctionTemplateValuePackTag<0>, int>{}) != 278) {
        return 137;
    }
    if (choose_member_call_template_double_explicit_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                4, FunctionTemplateMemberCallTemplate,
                FunctionTemplateValuePackTag<3>, short>{}) != 277) {
        return 138;
    }
    if (choose_member_call_floating_template_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallTemplate, int>{}) != 280) {
        return 139;
    }
    if (choose_member_call_floating_template_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                3, FunctionTemplateMemberCallTemplate, short>{}) != 279) {
        return 140;
    }
    if (choose_member_call_floating_overload_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                20, FunctionTemplateMemberCallFloatingOverload, int>{}) !=
        281) {
        return 141;
    }
    if (choose_member_call_floating_to_integer_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                12, FunctionTemplateMemberCallFloatingToInteger, int>{}) !=
        283) {
        return 142;
    }
    if (choose_member_call_nontype_pack_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                3, FunctionTemplateMemberCallNonTypePack,
                FunctionTemplateValuePackTag<1>,
                FunctionTemplateValuePackTag<2> >{}) != 285) {
        return 144;
    }
    if (choose_member_call_nontype_pack_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                0, FunctionTemplateMemberCallNonTypePack>{}) != 285) {
        return 145;
    }
    if (choose_member_call_nontype_pack_expression_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                5, FunctionTemplateMemberCallNonTypePack,
                FunctionTemplateValuePackTag<1>,
                FunctionTemplateValuePackTag<2> >{}) != 287) {
        return 146;
    }
    if (choose_member_call_nontype_pack_expression_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                0, FunctionTemplateMemberCallNonTypePack>{}) != 287) {
        return 147;
    }
    if (choose_member_call_mixed_overload_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                3, FunctionTemplateMemberCallMixedOverload,
                FunctionTemplateValuePackTag<2>, short>{}) != 268) {
        return 118;
    }
    if (choose_member_call_mixed_overload_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                5, FunctionTemplateMemberCallMixedOverload,
                FunctionTemplateValuePackTag<4>, short>{}) != 267) {
        return 119;
    }
    if (choose_member_call_variadic_competitor_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                5, FunctionTemplateMemberCallVariadicCompetitor, short>{}) !=
        289) {
        return 148;
    }
    if (choose_member_call_variadic_competitor_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallVariadicCompetitor, short>{}) !=
        290) {
        return 149;
    }
    if (choose_member_call_variadic_ordinary_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                5, FunctionTemplateMemberCallVariadicOrdinary, short>{}) !=
        291) {
        return 150;
    }
    if (choose_member_call_variadic_ordinary_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallVariadicOrdinary, short>{}) !=
        292) {
        return 151;
    }
    if (choose_member_call_variadic_member_template_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                3, FunctionTemplateMemberCallVariadicMemberTemplate,
                short>{}) != 293) {
        return 154;
    }
    if (choose_member_call_variadic_member_template_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                1, FunctionTemplateMemberCallVariadicMemberTemplate,
                short>{}) != 294) {
        return 155;
    }
    if (choose_member_call_variadic_member_template_competitor_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                5,
                FunctionTemplateMemberCallVariadicMemberTemplateCompetitor,
                short>{}) != 295) {
        return 156;
    }
    if (choose_member_call_floating_variadic_member_template_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                3, FunctionTemplateMemberCallVariadicMemberTemplate,
                short>{}) != 297) {
        return 157;
    }
    if (choose_member_call_aggregate_variadic_member_template_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                3, FunctionTemplateMemberCallVariadicMemberTemplate,
                short>{}) != 299) {
        return 170;
    }
    if (choose_member_call_mixed_overload_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                3, FunctionTemplateMemberCallOrdinaryFallback,
                FunctionTemplateValuePackTag<2>, short>{}) != 268) {
        return 120;
    }
    if (choose_member_call_mixed_overload_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                5, FunctionTemplateMemberCallOrdinaryFallback,
                FunctionTemplateValuePackTag<4>, short>{}) != 267) {
        return 121;
    }
    if (choose_member_call_noninteger_competitor_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                3, FunctionTemplateMemberCallNonIntegerOverload,
                FunctionTemplateValuePackTag<2>, short>{}) != 270) {
        return 122;
    }
    if (choose_member_call_noninteger_competitor_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                5, FunctionTemplateMemberCallNonIntegerOverload,
                FunctionTemplateValuePackTag<4>, short>{}) != 269) {
        return 123;
    }
    if (choose_inherited_member_call_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                3, FunctionTemplateMemberCallInheritedDerived,
                FunctionTemplateValuePackTag<2>, short>{}) != 272) {
        return 124;
    }
    if (choose_inherited_member_call_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                5, FunctionTemplateMemberCallInheritedDerived,
                FunctionTemplateValuePackTag<4>, short>{}) != 271) {
        return 125;
    }
    if (choose_inherited_member_call_template_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                4, FunctionTemplateMemberCallInheritedTemplateDerived,
                FunctionTemplateValuePackTag<3>, short>{}) != 274) {
        return 126;
    }
    if (choose_inherited_member_call_template_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                5, FunctionTemplateMemberCallInheritedTemplateDerived,
                FunctionTemplateValuePackTag<4>, short>{}) != 273) {
        return 127;
    }
    if (choose_inherited_member_call_template_explicit_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                4, FunctionTemplateMemberCallInheritedTemplateDerived,
                FunctionTemplateValuePackTag<3>, short>{}) != 276) {
        return 128;
    }
    if (choose_inherited_member_call_template_explicit_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                5, FunctionTemplateMemberCallInheritedTemplateDerived,
                FunctionTemplateValuePackTag<4>, short>{}) != 275) {
        return 129;
    }
    FunctionTemplateMemberCallInheritedTemplateDerived inherited_template;
    inherited_template.base_value = 1;
    if (inherited_template.adjust(3) != 4) {
        return 130;
    }
    FunctionTemplateMemberCallInheritedTemplateVirtualDerived
        virtual_inherited_template;
    virtual_inherited_template.base_value = 1;
    if (virtual_inherited_template.adjust(4) != 5) {
        return 131;
    }
    FunctionTemplateMemberCallInheritedTemplateMultipleDerived
        multiple_inherited_template;
    multiple_inherited_template.padding = 0;
    multiple_inherited_template.base_value = 2;
    if (multiple_inherited_template.adjust(3) != 5) {
        return 132;
    }
    if (multiple_inherited_template.adjust<long>(3) != 5) {
        return 134;
    }
    FunctionTemplateMemberCallInheritedTemplateUsingDerived
        using_inherited_template;
    using_inherited_template.padding = 0;
    using_inherited_template.base_value = 2;
    using_inherited_template.other_value = 50;
    if (using_inherited_template.adjust(3) != 5) {
        return 133;
    }
    const int explicit_double_result =
        using_inherited_template.adjust<double>(3);
    if (explicit_double_result != 5) return 135;
    if (cxx_integer_return_as_double(5) != 5.0) return 136;
    if (choose_member_arithmetic_value_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                2, FunctionTemplateMemberValueOne>{}) != 238) {
        return 83;
    }
    if (choose_member_arithmetic_value_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                4, FunctionTemplateMemberValueThree>{}) != 237) {
        return 84;
    }
    if (choose_member_conditional_value_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                2, FunctionTemplateMemberValueOne>{}) != 240) {
        return 85;
    }
    if (choose_member_conditional_value_class_pack_order(
            FunctionTemplateMemberValuePackBox<
                5, FunctionTemplateMemberValueThree>{}) != 239) {
        return 86;
    }
    if (choose_pack_size_class_pack_order(
            FunctionTemplateDependentValuePackBox<2, int, short>{}) != 242) {
        return 87;
    }
    if (choose_pack_size_class_pack_order(
            FunctionTemplateDependentValuePackBox<1, int, char>{}) != 241) {
        return 88;
    }
    if (choose_pack_size_class_pack_order(
            FunctionTemplateDependentValuePackBox<0, int>{}) != 241) {
        return 89;
    }
    short priority = 0;
    return choose_template(&value) == 25 &&
                   choose_template(value) == 10 &&
                   choose_fixed_over_pack(value) == 110 &&
                   choose_fixed_prefix_over_pack(value, value) == 130 &&
                   choose_pack_pointer_partial_order(pointer, value) == 146 &&
                   choose_pointer_expansion_partial_order() == 201 &&
                   choose_pointer_expansion_partial_order(pointer) == 202 &&
                   choose_pointer_expansion_partial_order(pointer,
                                                          &priority) == 202 &&
                   choose_pointer_expansion_partial_order(pointer,
                                                          &priority,
                                                          pointer) == 202 &&
                   choose_nontrailing_function_pack(7) == 203 &&
                   choose_nontrailing_function_pack<short>(
                       (short)4, 7) == 203 &&
                   choose_nontrailing_function_pack<int, short>(
                       4, (short)5, 7) == 203 &&
                   choose_nontrailing_pointer_function_pack<int, short>(
                       pointer, &priority, 7) == 9 &&
                   choose_nontrailing_pointer_pack_with_deduced_suffix<
                       int, short>(pointer, &priority, 7) == 9 &&
                   choose_nontrailing_pointer_pack_with_default_suffix<
                       int, short>(pointer, &priority, 2u) == 242 &&
                   choose_nontrailing_pointer_pack_with_pack_dependent_default_suffix<
                       int, short>(pointer, &priority, 2u) == 222 &&
                   choose_nontrailing_pointer_pack_with_pack_dependent_default_suffix<>(
                       2u) == 12 &&
                   choose_nontrailing_pointer_pack_with_non_type_pack_dependent_default_suffix<
                       int, short>(pointer, &priority, 2u) == 222 &&
                   choose_nontrailing_pointer_pack_with_non_type_pack_dependent_default_suffix<>(
                       2u) == 12 &&
                   choose_template_template_pack_with_partial_default_suffix<
                       FunctionTemplateTemplatePartialA,
                       FunctionTemplateTemplatePartialB>(2u) == 222 &&
                   choose_template_template_pack_with_partial_default_suffix<>(
                       2u) == 12 &&
                   choose_non_type_pack_partial_order(
                       FunctionTemplateValuePackTag<1>{}) == 171 &&
                   choose_non_type_pack_partial_order(
                       FunctionTemplateValuePackTag<1>{},
                       FunctionTemplateValuePackTag<2>{}) == 171 &&
                   choose_non_type_pack_partial_order<1>(
                       FunctionTemplateValuePackTag<1>{}) == 171 &&
                   choose_non_type_pack_partial_order<1, 2>(
                       FunctionTemplateValuePackTag<1>{},
                       FunctionTemplateValuePackTag<2>{}) == 171 &&
                    choose_non_type_class_pack_partial_order(
                        FunctionTemplateNonTypePackBox<>{}) == 182 &&
                    choose_non_type_class_pack_partial_order(
                        FunctionTemplateNonTypePackBox<1>{}) == 183 &&
                    choose_non_type_class_pack_partial_order(
                        FunctionTemplateNonTypePackBox<1, 2>{}) == 183 &&
                    choose_class_template_template_pack_expansion_order(
                        FunctionTemplateTemplatePackBox<>{}) == 276 &&
                    choose_class_template_template_pack_expansion_order(
                        FunctionTemplateTemplatePackBox<
                            FunctionTemplateTemplatePackItem>{}) == 277 &&
                    choose_class_template_template_pack_expansion_order(
                        FunctionTemplateTemplatePackBox<
                            FunctionTemplateTemplatePackItem,
                            FunctionTemplateTemplatePackFixed>{}) == 277 &&
                   choose_expression_non_type_pack_partial_order<1>(
                       FunctionTemplateExpressionValuePackTag<2>{}) == 181 &&
                   choose_expression_non_type_pack_partial_order<1, 2>(
                       FunctionTemplateExpressionValuePackTag<2>{},
                       FunctionTemplateExpressionValuePackTag<3>{}) == 181 &&
                   choose_nontrailing_non_type_pack_order<3>(
                       FunctionTemplateNonTypePackBox<3, 7>{}) == 272 &&
                   choose_nontrailing_non_type_pack_order(
                       FunctionTemplateNonTypePackBox<3, 5>{}) == 273 &&
                   choose_dependent_non_type_partial_order(
                       FunctionTemplateTwoValueTag<4, 4>{}) == 191 &&
                   choose_fixed_non_type_partial_order(
                       FunctionTemplateTwoValueTag<4, 7>{}) == 193 &&
                   choose_template_template_partial_order(
                       FunctionTemplatePartialOrderBox<int>{}) == 195 &&
                   choose_template_template_partial_order(
                       FunctionTemplatePartialOrderBox<int*>{}) == 196 &&
                   choose_template_pack_partial_order(
                       FunctionTemplatePackPartialOrderBox<int>{}) == 197 &&
                   choose_template_pack_partial_order(
                       FunctionTemplatePackPartialOrderBox<int*>{}) == 198 &&
                   choose_template_class_pack_partial_order(
                       FunctionTemplatePackPartialOrderBox<int, int>{}) ==
                       199 &&
                   choose_template_class_pack_partial_order(
                       FunctionTemplatePackPartialOrderBox<int*, int>{}) ==
                       200 &&
                   count_template_class_pack_expansion(
                       FunctionTemplatePackPartialOrderBox<int>{}) == 210 &&
                   count_template_class_pack_expansion(
                       FunctionTemplatePackPartialOrderBox<int, short>{}) ==
                       211 &&
                   count_template_class_pack_expansion(
                       FunctionTemplatePackPartialOrderBox<int, short,
                                                           long>{}) == 212 &&
                   choose_class_template_pack_expansion_order(
                       FunctionTemplatePackPartialOrderBox<>{}) == 213 &&
                   choose_class_template_pack_expansion_order(
                       FunctionTemplatePackPartialOrderBox<int>{}) == 214 &&
                   choose_class_template_pack_expansion_order(
                       FunctionTemplatePackPartialOrderBox<int, short>{}) ==
                       214 &&
                   choose_nontrailing_template_argument_pack_order<char>(
                       FunctionTemplatePackPartialOrderBox<char, int>{}) ==
                       268 &&
                   choose_nontrailing_template_argument_pack_order(
                       FunctionTemplatePackPartialOrderBox<char, double>{}) ==
                       269 &&
                   choose_repeated_class_template_pack_expansions(
                       FunctionTemplatePackPartialOrderBox<char, short, int,
                                                           char, short>{},
                       FunctionTemplatePackPartialOrderBox<char, short>{}) ==
                       2 &&
                   choose_repeated_class_template_pack_expansions(
                       FunctionTemplatePackPartialOrderBox<int>{},
                       FunctionTemplatePackPartialOrderBox<>{}) == 0 &&
                   choose_repeated_class_non_type_pack_expansions(
                       FunctionTemplateNonTypePackBox<1, 2, 7, 1, 2>{},
                       FunctionTemplateNonTypePackBox<1, 2>{}) == 2 &&
                   choose_repeated_class_non_type_pack_expansions(
                       FunctionTemplateNonTypePackBox<7>{},
                       FunctionTemplateNonTypePackBox<>{}) == 0 &&
                   choose_repeated_class_template_template_pack_expansions(
                       FunctionTemplateTemplatePackBox<
                           FunctionTemplateTemplatePackItem,
                           FunctionTemplateTemplatePackFixed,
                           FunctionTemplateTemplatePackItem>{},
                       FunctionTemplateTemplatePackBox<
                           FunctionTemplateTemplatePackItem>{}) == 1 &&
                   choose_repeated_class_template_template_pack_expansions(
                       FunctionTemplateTemplatePackBox<
                           FunctionTemplateTemplatePackFixed>{},
                       FunctionTemplateTemplatePackBox<>{}) == 0 &&
                   choose_interleaved_class_template_pack_sources<char>(
                       FunctionTemplatePackPartialOrderBox<
                           char, int, short, double>{},
                       FunctionTemplatePackPartialOrderBox<short,
                                                           double>{}) == 12 &&
                   choose_interleaved_class_template_pack_sources<>(
                       FunctionTemplatePackPartialOrderBox<int>{},
                       FunctionTemplatePackPartialOrderBox<>{}) == 0 &&
                   choose_interleaved_class_template_pack_sources(
                       FunctionTemplatePackPartialOrderBox<int, short,
                                                           double>{},
                       FunctionTemplatePackPartialOrderBox<short,
                                                           double>{}) == 2 &&
                   choose_independent_class_template_pack_partition(
                       FunctionTemplatePackPartialOrderBox<char>{},
                       FunctionTemplatePackPartialOrderBox<short>{}) == 11 &&
                   choose_independent_class_template_pack_partition(
                       FunctionTemplatePackPartialOrderBox<char, short>{},
                       FunctionTemplatePackPartialOrderBox<>{}) == 20 &&
                   choose_independent_class_template_pack_partition(
                       FunctionTemplatePackPartialOrderBox<>{},
                       FunctionTemplatePackPartialOrderBox<char, short>{}) ==
                       2 &&
                   choose_independent_non_type_class_pack_partition(
                       FunctionTemplateNonTypePackBox<1>{},
                       FunctionTemplateNonTypePackBox<2>{}) == 11 &&
                   choose_independent_non_type_class_pack_partition(
                       FunctionTemplateNonTypePackBox<1, 2>{},
                       FunctionTemplateNonTypePackBox<>{}) == 20 &&
                   choose_independent_non_type_class_pack_partition(
                       FunctionTemplateNonTypePackBox<>{},
                       FunctionTemplateNonTypePackBox<1, 2>{}) == 2 &&
                   choose_independent_template_template_class_pack_partition(
                       FunctionTemplateTemplatePackBox<
                           FunctionTemplateTemplatePackItem>{},
                       FunctionTemplateTemplatePackBox<
                           FunctionTemplateTemplatePackFixed>{}) == 11 &&
                   choose_independent_template_template_class_pack_partition(
                       FunctionTemplateTemplatePackBox<
                           FunctionTemplateTemplatePackItem,
                           FunctionTemplateTemplatePackFixed>{},
                       FunctionTemplateTemplatePackBox<>{}) == 20 &&
                   choose_independent_template_template_class_pack_partition(
                       FunctionTemplateTemplatePackBox<>{},
                       FunctionTemplateTemplatePackBox<
                           FunctionTemplateTemplatePackItem,
                           FunctionTemplateTemplatePackFixed>{}) == 2 &&
                   choose_nontrailing_template_template_pack_order<
                       FunctionTemplateTemplatePackItem>(
                       FunctionTemplateTemplatePackBox<
                           FunctionTemplateTemplatePackItem,
                           FunctionTemplateTemplatePackFixed>{}) == 274 &&
                   choose_nontrailing_template_template_pack_order(
                       FunctionTemplateTemplatePackBox<
                           FunctionTemplateTemplatePackItem,
                           FunctionTemplateTemplatePackItem>{}) == 275 &&
                   count_class_pointer_pack_expansion(
                       FunctionTemplatePackPartialOrderBox<>{}) == 220 &&
                   count_class_pointer_pack_expansion(
                       FunctionTemplatePackPartialOrderBox<int*>{}) == 221 &&
                   count_class_pointer_pack_expansion(
                       FunctionTemplatePackPartialOrderBox<int*, short*>{}) ==
                       222 &&
                   choose_mixed_value_class_order(
                       FunctionTemplateMixedValueBox<7, int*>{}) == 228 &&
                   choose_mixed_value_class_order(
                       FunctionTemplateMixedValueBox<7, int>{}) == 227 &&
                   choose_mixed_value_class_order(
                       FunctionTemplateMixedValueBox<7, const char*>{}) ==
                       228 &&
                   choose_mixed_value_class_pack_order(
                       FunctionTemplateMixedValuePackBox<7, int*>{}) == 230 &&
                   choose_mixed_value_class_pack_order(
                       FunctionTemplateMixedValuePackBox<7, int*, short*>{}) ==
                       230 &&
                   choose_mixed_value_class_pack_order(
                       FunctionTemplateMixedValuePackBox<7, int>{}) == 229 &&
                   choose_mixed_value_class_pack_order(
                       FunctionTemplateMixedValuePackBox<7>{}) == 230 &&
                   choose_dependent_value_class_pack_order(
                       FunctionTemplateDependentValuePackBox<
                           sizeof(int), int, int*>{}) == 232 &&
                   choose_dependent_value_class_pack_order(
                       FunctionTemplateDependentValuePackBox<
                           sizeof(short), short, int, short*>{}) == 231 &&
                   choose_dependent_value_class_pack_order(
                       FunctionTemplateDependentValuePackBox<
                           sizeof(long), long>{}) == 232 &&
                   choose_arithmetic_value_class_pack_order(
                       FunctionTemplateArithmeticValuePackBox<
                           sizeof(int) + 1, int, int*>{}) == 234 &&
                   choose_arithmetic_value_class_pack_order(
                       FunctionTemplateArithmeticValuePackBox<
                           sizeof(short) + 1, short, int, short*>{}) == 233 &&
                   choose_arithmetic_value_class_pack_order(
                       FunctionTemplateArithmeticValuePackBox<
                           sizeof(long) + 1, long>{}) == 234 &&
                   choose_compound_pointer_pack_order(
                       FunctionTemplatePackPartialOrderBox<>{}) == 223 &&
                   choose_compound_pointer_pack_order(
                       FunctionTemplatePackPartialOrderBox<int*, short>{}) ==
                       224 &&
                   choose_compound_pointer_over_plain_pack(
                       FunctionTemplatePackPartialOrderBox<>{}) == 225 &&
                   choose_compound_pointer_over_plain_pack(
                       FunctionTemplatePackPartialOrderBox<int*, short*>{}) ==
                       225 &&
                   choose_compound_pointer_over_plain_pack(
                       FunctionTemplatePackPartialOrderBox<int, short*>{}) ==
                       226 &&
                   choose_compound_class_pack_order(
                       FunctionTemplatePackPartialOrderBox<>{}) == 260 &&
                   choose_compound_class_pack_order(
                       FunctionTemplatePackPartialOrderBox<
                           FunctionTemplatePackCompoundElement<int>>{}) ==
                       260 &&
                   choose_compound_class_pack_order(
                       FunctionTemplatePackPartialOrderBox<
                           FunctionTemplatePackCompoundElement<int>,
                           FunctionTemplatePackCompoundElement<short>>{}) ==
                       260 &&
                   choose_compound_class_pack_order(
                       FunctionTemplatePackPartialOrderBox<
                           FunctionTemplatePackCompoundElement<int>, short>{}) ==
                       261 &&
                   choose_repeated_compound_class_pack_order(
                       FunctionTemplatePackPartialOrderBox<>{}) == 262 &&
                   choose_repeated_compound_class_pack_order(
                       FunctionTemplatePackPartialOrderBox<
                           FunctionTemplatePackRepeatedPair<int, int>>{}) ==
                       262 &&
                   choose_repeated_compound_class_pack_order(
                       FunctionTemplatePackPartialOrderBox<
                           FunctionTemplatePackRepeatedPair<int, int>,
                           FunctionTemplatePackRepeatedPair<short, short>>{}) ==
                       262 &&
                   choose_repeated_compound_class_pack_order(
                       FunctionTemplatePackPartialOrderBox<
                           FunctionTemplatePackRepeatedPair<int, short>>{}) ==
                       263 &&
                   choose_repeated_compound_class_pack_order(
                       FunctionTemplatePackPartialOrderBox<
                           FunctionTemplatePackRepeatedPair<int, int>, short>{}) ==
                       263 &&
                   choose_compound_pack_size_order(
                       FunctionTemplateDependentValuePackBox<0, int>{}) == 264 &&
                   choose_compound_pack_size_order(
                       FunctionTemplateDependentValuePackBox<
                           1, int, FunctionTemplatePackCompoundElement<short>>{}) ==
                       264 &&
                   choose_compound_pack_size_order(
                       FunctionTemplateDependentValuePackBox<
                           2, int,
                           FunctionTemplatePackCompoundElement<short>,
                           FunctionTemplatePackCompoundElement<long>>{}) == 264 &&
                   choose_compound_pack_size_order(
                       FunctionTemplateDependentValuePackBox<
                           2, int, FunctionTemplatePackCompoundElement<short>,
                           long>{}) == 265 &&
                   choose_compound_pack_size_plus_one_order(
                       FunctionTemplateDependentValuePackBox<1, int>{}) == 266 &&
                   choose_compound_pack_size_plus_one_order(
                       FunctionTemplateDependentValuePackBox<
                           2, int,
                           FunctionTemplatePackCompoundElement<short>>{}) == 266 &&
                   choose_compound_pack_size_plus_one_order(
                       FunctionTemplateDependentValuePackBox<
                           3, int,
                           FunctionTemplatePackCompoundElement<short>,
                           FunctionTemplatePackCompoundElement<long>>{}) == 266 &&
                   choose_compound_pack_size_plus_one_order(
                       FunctionTemplateDependentValuePackBox<
                           3, int,
                           FunctionTemplatePackCompoundElement<short>,
                           long>{}) == 267 &&
                   choose_pointer_pack_over_value(pointer) == 147 &&
                   choose_constraint_partial_order(constrained_value) == 150 &&
                   choose_template_template_constraint_order(
                       PackOrderTemplateBox<PackOrderConstrainedValue>{}) ==
                       154 &&
                   choose_constrained_template_template_signature_order(
                       PackOrderTemplateBox<PackOrderConstrainedValue>{}) ==
                       158 &&
                   choose_defaulted_template_template_constraint_order(
                       PackOrderDefaultedTemplateBox<
                           PackOrderConstrainedValue>{}) == 155 &&
                   choose_dependent_default_template_template_constraint_order(
                       PackOrderDependentDefaultTemplateBox<
                           PackOrderConstrainedValue>{}) == 159 &&
                   choose_non_type_default_template_template_order(
                       PackOrderNonTypeDefaultTemplateBox<
                           PackOrderConstrainedValue>{}) == 161 &&
                   choose_template_default_template_template_order(
                       PackOrderTemplateDefaultTemplateBox<
                           PackOrderConstrainedValue>{}) == 163 &&
                   choose_outer_non_type_default_template_template_order(
                       PackOrderOuterNonTypeDefaultTemplateBox<
                           PackOrderConstrainedValue, 4>{}) == 165 &&
                   choose_outer_non_type_expression_default_template_template_order(
                       PackOrderOuterNonTypeDefaultTemplateBox<
                           PackOrderConstrainedValue, 5>{},
                       constrained_values) == 167 &&
                   choose_outer_non_type_arithmetic_default_template_template_order(
                       PackOrderOuterNonTypeDefaultTemplateBox<
                           PackOrderConstrainedValue, 9>{},
                       constrained_values) == 171 &&
                   choose_outer_non_type_cast_default_template_template_order(
                       PackOrderOuterNonTypeDefaultTemplateBox<
                           PackOrderConstrainedValue, 14>{},
                       constrained_values) == 183 &&
                   choose_outer_non_type_concept_default_template_template_order(
                       PackOrderOuterNonTypeDefaultTemplateBox<
                           PackOrderConstrainedValue, 14>{},
                       constrained_values) == 187 &&
                   choose_outer_non_type_constexpr_call_default_template_template_order(
                       PackOrderOuterNonTypeDefaultTemplateBox<
                           PackOrderConstrainedValue, 14>{},
                       constrained_values) == 191 &&
                   PackOrderConstexprUsingLookupTemplate::
                       choose_using_lookup_constexpr_call_default_order(
                           PackOrderOuterNonTypeDefaultTemplateBox<
                               PackOrderConstrainedValue, 14>{},
                           constrained_values) == 223 &&
                   PackOrderConstexprUsingDirectiveTemplate::
                       choose_using_directive_constexpr_call_default_order(
                           PackOrderOuterNonTypeDefaultTemplateBox<
                               PackOrderConstrainedValue, 14>{},
                           constrained_values) == 227 &&
                   PackOrderConstexprEnumAdlTemplate::
                       select_default_value<seed>(
                           PackOrderOuterNonTypeDefaultTemplateBox<
                               PackOrderConstrainedValue, 12>{}) == 12 &&
                   PackOrderConstexprInlineEnumAdlTemplate::
                       select_default_value<
                           inline_seed>(
                           PackOrderOuterNonTypeDefaultTemplateBox<
                               PackOrderConstrainedValue, 12>{}) == 12 &&
                   PackOrderConstexprInlineEnumAdlTemplate::
                       qualified_enumerator_value<
                           PackOrderConstexprInlineEnumAdl::V1::
                               inline_seed>() == 2 &&
                   PackOrderConstexprInlineEnumAdlTemplate::
                       qualified_enumerator_value<
                           PackOrderConstexprInlineEnumAdl::
                               inline_seed>() == 2 &&
                   PackOrderConstexprInlineEnumAdlTemplate::
                       qualified_enumerator_value<
                           PackOrderInlineEnumAdlAlias::
                               inline_seed>() == 2 &&
                   choose_outer_non_type_constexpr_template_call_default_template_template_order(
                       PackOrderOuterNonTypeDefaultTemplateBox<
                           PackOrderConstrainedValue, 14>{},
                       constrained_values) == 195 &&
                   choose_outer_non_type_overloaded_constexpr_template_call_default_order(
                       PackOrderOuterNonTypeDefaultTemplateBox<
                           PackOrderConstrainedValue, 14>{},
                       constrained_values) == 199 &&
                   choose_nested_type_default_template_template_order(
                       PackOrderNestedTypeDefaultTemplateBox<
                           int, unsigned>{},
                       nested_type_default_value) == 175 &&
                   choose_nested_deep_type_default_template_template_order(
                       PackOrderNestedTypeDefaultDeepTemplateBox<
                           int, unsigned>{},
                       nested_deep_type_default_value) == 179 &&
                   choose_constraint_disjunction_partial_order(
                       constrained_value) == 152 &&
                   choose_nonpack_over_empty_pack(value) == 160 &&
                   forward_template(choose_template(&value)) == 25 &&
                   choose_conversion_before_partial_order(&value, priority) == 10 &&
                   choose_cv_qualification(pointer) == 30 &&
                   choose_reference_binding(value) == 51 &&
                   choose_reference_binding(0) == 52 &&
                   choose_reference_cv_category(value) == 171 &&
                   choose_reference_cv_category(const_value) == 172 &&
                   choose_reference_qualification(value) == 231 &&
                   choose_reference_pointer_qualification(pointer) == 246 &&
                   choose_reference_qualification_template(value, value) ==
                       241 &&
                   choose_reference_pointer_qualification_template(
                       value, pointer) == 244 &&
                   choose_rvalue_cv_category(0) == 173 &&
                   choose_rvalue_cv_category(
                       static_cast<const int&&>(const_value)) == 174 &&
                   choose_pointer_bool(pointer, pointer) == 62 &&
                   choose_pointer_subsequence(pointer, pointer) == 71 &&
                   choose_pointer_subsequence_non_template(pointer) == 73 &&
                   choose_same_conversion_trailing_sequence(
                       conversion_pointer) == 241 &&
                   choose_same_conversion_trailing_sequence_template(
                       value, conversion_pointer) == 243 &&
                   choose_nested_pointer_qualification(pointer_to_pointer) ==
                       75 &&
                   choose_array_bound_reference(array_value) == 25 &&
                   choose_array_bound_reference_template(array_value) == 45 &&
                   choose_array_bound_reference_cv(array_value) == 65 &&
                   choose_array_bound_reference_cv_template(array_value) ==
                       85 &&
                   choose_template_array_extent(array_value) == 175 &&
                   choose_template_array_pointer_extent(array_pointer) ==
                       195 &&
                   choose_template_nested_array_pointer_extent(
                       matrix_pointer) == 215 &&
                   choose_base_over_void_non_template(leaf_pointer) == 97 &&
                   choose_nearer_base(leaf_pointer, leaf_pointer) == 82 &&
                   choose_nearer_base_reference(leaf_pointer, leaf) == 86 &&
                   choose_cv_nearer_base_reference(leaf) == 103 &&
                   choose_cv_nearer_base_reference_template(
                       leaf_pointer, leaf) == 105 &&
                   choose_cv_derived_base_reference(leaf) == 107 &&
                   choose_cv_derived_base_reference_template(
                       leaf_pointer, leaf) == 109 &&
                   choose_nearer_base_reference_non_template(leaf) == 88 &&
                   choose_nearer_base_pointer_non_template(leaf_pointer) ==
                       90 &&
                   choose_derived_base_qualification(leaf_pointer) == 99 &&
                   choose_derived_base_qualification_template(
                       pointer, leaf_pointer) == 101 &&
                   choose_cv_nearer_base_pointer(leaf_pointer) == 112 &&
                   choose_cv_nearer_base_pointer_template(
                       pointer, leaf_pointer) == 114 &&
                   choose_cv_incomparable_nearer_base_pointer(
                       leaf_pointer) == 115 &&
                   choose_cv_incomparable_nearer_base_pointer_template(
                       pointer, leaf_pointer) == 117 &&
                   choose_fixed_enum_promotion(
                       conversion_fixed_byte_value) == 211 &&
                   choose_fixed_enum_template(
                       value, conversion_fixed_byte_value) == 213 &&
                   choose_pointer_to_bool_non_template(pointer) == 216 &&
                   verify_array_to_bool_decay() == 245 &&
                   verify_array_to_bool_reference_decay() == 249 &&
                   verify_function_to_bool_decay() == 247 &&
                   verify_function_to_bool_reference_decay() == 251 &&
                   verify_array_to_bool_rvalue_reference_decay() == 253 &&
                   verify_function_to_bool_rvalue_reference_decay() == 255 &&
                   verify_object_pointer_to_bool_reference() == 257 &&
                   verify_function_pointer_to_bool_reference() == 257 &&
                   verify_object_pointer_to_bool_rvalue_reference() == 259 &&
                   verify_function_pointer_to_bool_rvalue_reference() == 259 &&
                   verify_data_member_pointer_to_bool() &&
                   verify_function_member_pointer_to_bool() &&
                   verify_member_pointer_contextual_bool() &&
                   choose_nullptr_to_bool_non_template(nullptr) == 218 &&
                   choose_nullptr_to_bool_template(value, nullptr) == 220 &&
                   !static_cast<bool>(nullptr) &&
                   choose_noexcept_function_pointer(noexcept_pointer) == 222 &&
                   choose_noexcept_function_pointer_template(
                       value, noexcept_pointer) == 224 &&
                   choose_noexcept_function_pointer(
                       conversion_noexcept_function) == 222 &&
                   choose_noexcept_function_pointer_template(
                       value, conversion_noexcept_function) == 224 &&
                   choose_noexcept_function_pointer(
                       conversion_overloaded_noexcept_function) == 222 &&
                   choose_noexcept_function_pointer_template(
                       value,
                       conversion_overloaded_noexcept_function) == 224 &&
                   ConversionFunctionOverloadNamespace::
                       verify_function_pointer_overload_context() &&
                   ConversionFunctionUsingDeclarationNamespace::
                       verify_function_pointer_overload_context() &&
                   ConversionFunctionUsingDirectiveNamespace::
                       verify_function_pointer_overload_context() &&
                   verify_function_template_pointer_overload() &&
                   verify_function_template_using_lookup() &&
                   verify_noexcept_function_template_rank() &&
                   choose_noexcept_function_pointer(
                       explicit_noexcept_pointer) == 222 &&
                   choose_noexcept_function_pointer_template(
                       value, explicit_noexcept_pointer) == 224 &&
                   choose_noexcept_function_pointer(
                       explicit_throwing_pointer) == 221 &&
                   choose_noexcept_function_pointer_template(
                       value, explicit_throwing_pointer) == 223 &&
                   choose_noexcept_function_pointer(
                       conversion_explicit_noexcept_function) == 222 &&
                   choose_noexcept_function_pointer_template(
                       value, conversion_explicit_noexcept_function) == 224 &&
                   choose_noexcept_function_pointer(
                       conversion_explicit_throwing_function) == 221 &&
                   choose_noexcept_function_pointer_template(
                       value, conversion_explicit_throwing_function) == 223 &&
                   choose_nearer_member_pointer(
                       0, &ConversionMemberPointerRoot::value) == 191 &&
                   choose_nearer_member_pointer_non_template(
                       &ConversionMemberPointerRoot::value) == 193 &&
                   choose_nearer_member_function_pointer(
                       0, root_member_function) == 195 &&
                   choose_nearer_member_function_pointer_non_template(
                       root_member_function) == 197 &&
                   choose_member_pointer_member_type_cv(
                       0, &ConversionMemberPointerRoot::value) == 201 &&
                   choose_member_pointer_member_type_cv_non_template(
                       &ConversionMemberPointerRoot::value) == 203 &&
                   choose_member_pointer_owner_cv_subsequence(
                       0, &ConversionMemberPointerRoot::value) == 233 &&
                   choose_member_pointer_owner_cv_subsequence_non_template(
                       &ConversionMemberPointerRoot::value) == 235 &&
                   choose_nested_member_pointer_qualification(
                       0, &ConversionNestedMemberPointerOwner::pointer) ==
                       245 &&
                   choose_nested_member_pointer_qualification_non_template(
                       &ConversionNestedMemberPointerOwner::pointer) == 247 &&
                   choose_member_function_pointer_noexcept(
                       0, root_noexcept_member_function) == 205 &&
                   choose_member_function_pointer_noexcept(
                       0, root_throwing_member_function) == 206 &&
                   choose_member_function_pointer_noexcept_non_template(
                       root_noexcept_member_function) == 207 &&
                   choose_member_function_pointer_noexcept_non_template(
                       root_throwing_member_function) == 208 &&
                   choose_member_function_pointer_owner_noexcept(
                       0, root_noexcept_member_function) == 219 &&
                   choose_member_function_pointer_owner_noexcept_non_template(
                       root_noexcept_member_function) == 221 &&
                   choose_member_function_pointer_owner_noexcept_tradeoff(
                       0, root_noexcept_member_function) == 223 &&
                   choose_member_function_pointer_owner_noexcept_tradeoff_non_template(
                       root_noexcept_member_function) == 225 &&
                   choose_member_pointer_owner_cv_tradeoff(
                       0, &ConversionMemberPointerRoot::value) == 227 &&
                   choose_member_pointer_owner_cv_tradeoff_non_template(
                       &ConversionMemberPointerRoot::value) == 229 &&
                   choose_member_pointer_owner_cv_reverse_tradeoff(
                       0, &ConversionMemberPointerRoot::value) == 231 &&
                   choose_member_pointer_owner_cv_reverse_tradeoff_non_template(
                       &ConversionMemberPointerRoot::value) == 233 &&
                   choose_member_pointer_owner_qualification_tradeoff(
                       0, &ConversionMemberPointerRoot::value) == 235 &&
                   choose_member_pointer_owner_qualification_tradeoff_non_template(
                       &ConversionMemberPointerRoot::value) == 237 &&
                   choose_member_pointer_owner_qualification_reverse_tradeoff(
                       0, &ConversionMemberPointerRoot::value) == 239 &&
                   choose_member_pointer_owner_qualification_reverse_tradeoff_non_template(
                       &ConversionMemberPointerRoot::value) == 241 &&
                   invoke_private_member_pointer_owner(
                       &ConversionMemberPointerRoot::value) == 243 &&
                   invoke_private_member_pointer_owner_template(
                       &ConversionMemberPointerRoot::value) == 244 &&
                   choose_base_over_void(leaf_pointer, leaf_pointer) == 83 &&
                   choose_nearer_base_value(
                       value_leaf_pointer, value_leaf) == 92 &&
                   choose_nearer_base_value_non_template(value_leaf) == 94 &&
                   choose_nearer_base_value_non_template(
                       make_conversion_value_leaf()) == 94 &&
                   root_copy.root_value == 41 &&
                   cleanup_root_value == 53 &&
                   virtual_cleanup_count_before == cleanup_count_before + 1
               ? 0
               : 1;
}
