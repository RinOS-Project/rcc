class Meter {
public:
    int value;

    operator int() {
        return value;
    }
};

struct ScalarReferenceConversionSource {
    int value;

    operator int&() {
        return value;
    }
};

struct MemberPointerReferencePadding {
    int padding;
};

struct MemberPointerReferenceBase {
    int value;
};

struct MemberPointerReferenceDerived : MemberPointerReferencePadding,
                                       MemberPointerReferenceBase {};

struct MemberPointerReferenceLeaf : MemberPointerReferenceDerived {};

struct MemberPointerNestedQualificationOwner {
    int* pointer;
};

using MemberPointerNestedQualification =
    int* MemberPointerNestedQualificationOwner::*;
using MemberPointerNestedConstQualification =
    const int* MemberPointerNestedQualificationOwner::*;

struct MemberPointerArrayOwner {
    int values[2];
};

using MemberPointerArray = int (MemberPointerArrayOwner::*)[2];
using ConstMemberPointerArray = const int (MemberPointerArrayOwner::*)[2];

struct MemberPointerNestedArrayOwner {
    int* values[2];
};

using MemberPointerNestedArray =
    int* (MemberPointerNestedArrayOwner::*)[2];
using ConstPointeeMemberPointerNestedArray =
    const int* (MemberPointerNestedArrayOwner::*)[2];

struct MemberPointerAccessBase {
    int value;
};

struct MemberPointerPrivateOwner : private MemberPointerAccessBase {
    int value;
};

struct MemberPointerVirtualOwner : virtual MemberPointerAccessBase {
    int value;
};

using MemberPointerBaseValue = int MemberPointerAccessBase::*;
using MemberPointerPrivateValue = int MemberPointerPrivateOwner::*;
using MemberPointerVirtualValue = int MemberPointerVirtualOwner::*;

struct MemberPointerReferenceSource {
    int MemberPointerReferenceBase::*member;

    operator int MemberPointerReferenceBase::*() const {
        return member;
    }
};

struct MemberPointerNestedQualificationSource {
    operator MemberPointerNestedQualification() const {
        return &MemberPointerNestedQualificationOwner::pointer;
    }
};

struct MemberPointerArraySource {
    operator MemberPointerArray() const {
        return &MemberPointerArrayOwner::values;
    }
};

struct MemberPointerNestedArraySource {
    operator MemberPointerNestedArray() const {
        return &MemberPointerNestedArrayOwner::values;
    }
};

struct MemberPointerPrivateSource {
    operator MemberPointerPrivateValue() const {
        return &MemberPointerPrivateOwner::value;
    }
};

struct MemberPointerVirtualSource {
    operator MemberPointerVirtualValue() const {
        return &MemberPointerVirtualOwner::value;
    }
};

int consume_base_member_pointer(MemberPointerBaseValue);
int consume_const_pointee_member_pointer_array(
    ConstPointeeMemberPointerNestedArray);

#if defined(TEST_PRIVATE_MEMBER_POINTER_OWNER_CONVERSION)
int test_private_member_pointer_owner_conversion() {
    return consume_base_member_pointer(MemberPointerPrivateSource{});
}
#endif

#if defined(TEST_VIRTUAL_MEMBER_POINTER_OWNER_CONVERSION)
int test_virtual_member_pointer_owner_conversion() {
    return consume_base_member_pointer(MemberPointerVirtualSource{});
}
#endif

#if defined(TEST_NESTED_ARRAY_MEMBER_POINTER_QUALIFICATION)
int test_nested_array_member_pointer_qualification() {
    return consume_const_pointee_member_pointer_array(
        MemberPointerNestedArraySource{});
}
#endif

class Flag {
public:
    int value;

    explicit operator bool() {
        return value != 0;
    }
};

class RankedMeter {
public:
    operator float() {
        return 13.0f;
    }

    operator double() {
        return 17.0;
    }

    operator int() {
        return 19;
    }
};

class PromotionMeter {
public:
    operator short() {
        return 23;
    }

    operator long() {
        return 29;
    }
};

class FloatingPromotionMeter {
public:
    operator float() {
        return 31.0f;
    }

    operator int() {
        return 37;
    }
};

class ConversionObjectVsResultSource {
public:
    operator int() & {
        return 103;
    }

    operator long() const & {
        return 107;
    }
};

class TemplatedConversionSource {
public:
    template <class T>
    operator T() const {
        return static_cast<T>(67);
    }
};

class ConstrainedTemplatedConversionSource {
public:
    template <class T>
        requires (sizeof(T) == 4)
    operator T() const {
        return static_cast<T>(73);
    }
};

struct ReferenceConversionResult {
    int value;
};

struct ConversionBasePointerTarget {
    int value;
};

struct ConversionDerivedPointerTarget : ConversionBasePointerTarget {};

struct ConversionReferenceBase {
    int value;
};

struct ConversionReferenceMiddle : ConversionReferenceBase {};

struct ConversionReferenceLeaf : ConversionReferenceMiddle {};

class ConversionReferenceRankingSource {
public:
    ConversionReferenceMiddle middle;
    ConversionReferenceLeaf leaf;

    operator ConversionReferenceMiddle&() {
        return middle;
    }

    operator ConversionReferenceLeaf&() {
        return leaf;
    }
};

class ConversionPointerSource {
public:
    ConversionDerivedPointerTarget* value;

    operator ConversionDerivedPointerTarget*() const {
        return value;
    }
};

class ConversionPointerToBoolSource {
public:
    int* value;

    operator int*() const {
        return value;
    }
};

struct ConversionMemberPointerToBoolTarget {
    int value;
};

using ConversionMemberPointerToBool =
    int ConversionMemberPointerToBoolTarget::*;

class ConversionMemberPointerToBoolSource {
public:
    ConversionMemberPointerToBool value;

    operator ConversionMemberPointerToBool() const {
        return value;
    }
};

struct ConversionMemberFunctionSignatureTarget {
    int exact(int value) noexcept { return value + 10; }
    int different_parameter(long value) noexcept {
        return static_cast<int>(value) + 20;
    }
};

using ConversionMemberFunctionInt =
    int (ConversionMemberFunctionSignatureTarget::*)(int) noexcept;
using ConversionMemberFunctionLong =
    int (ConversionMemberFunctionSignatureTarget::*)(long) noexcept;
using ConversionMemberFunctionThrowing =
    int (ConversionMemberFunctionSignatureTarget::*)(int);

struct ConversionMemberFunctionSignatureSource {
    operator ConversionMemberFunctionInt() const {
        return &ConversionMemberFunctionSignatureTarget::exact;
    }
};

class ReferenceConversionSource {
public:
    operator ReferenceConversionResult() && {
        return {43};
    }
};

class RefQualifiedConversionSource {
public:
    operator ReferenceConversionResult() & {
        return {47};
    }

    operator ReferenceConversionResult() && {
        return {53};
    }
};

class CvRefQualifiedConversionSource {
public:
    operator ReferenceConversionResult() const & {
        return {59};
    }

    operator ReferenceConversionResult() && {
        return {61};
    }
};

using FunctionReferenceConversionTarget = int();
using ArrayReferenceConversionTarget = int[2];

int call_lvalue_function_conversion() {
    return 97;
}

int call_rvalue_function_conversion() {
    return 101;
}

class FunctionDesignatorToBoolSource {
public:
    operator FunctionReferenceConversionTarget&() const {
        return call_lvalue_function_conversion;
    }
};

class FunctionReferenceConversionSource {
public:
    operator FunctionReferenceConversionTarget&() const {
        return call_lvalue_function_conversion;
    }

    operator FunctionReferenceConversionTarget&&() const {
        return static_cast<FunctionReferenceConversionTarget&&>(
            call_rvalue_function_conversion);
    }
};

class RvalueFunctionReferenceSource {
public:
    operator FunctionReferenceConversionTarget&&() const {
        return static_cast<FunctionReferenceConversionTarget&&>(
            call_rvalue_function_conversion);
    }
};

class LvalueFunctionReferenceSource {
public:
    operator FunctionReferenceConversionTarget&() const {
        return call_lvalue_function_conversion;
    }
};

class ArrayReferenceConversionSource {
public:
    ArrayReferenceConversionTarget* value;

    operator ArrayReferenceConversionTarget&() const {
        return *value;
    }
};

int take_integer(int value) {
    return value;
}

int read_scalar_conversion_reference(const int& value) {
    return value;
}

int increment_scalar_conversion_reference(int& value) {
    return ++value;
}

int read_member_pointer_conversion_reference(
    MemberPointerReferenceDerived& object,
    int MemberPointerReferenceDerived::* const& member) {
    return object.*member;
}

int read_member_pointer_conversion_value(
    int MemberPointerReferenceBase::* member) {
    MemberPointerReferenceBase object{211};
    return object.*member;
}

int read_derived_member_pointer_conversion_value(
    MemberPointerReferenceDerived& object,
    int MemberPointerReferenceDerived::* member) {
    return object.*member;
}

int is_null_member_pointer_conversion_reference(
    int MemberPointerReferenceDerived::* const& member) {
    return member == nullptr;
}

int take_ranked_integer(int value) {
    return value;
}

int take_promoted_integer(int value) {
    return value;
}

double take_promoted_double(double value) {
    return value;
}

int take_function_pointer(FunctionReferenceConversionTarget* function) {
    return function();
}

int sum_array_decay(const int* values) {
    return values[0] + values[1];
}

int choose_conversion_reference(ReferenceConversionResult&& value) {
    return value.value + 100;
}

int choose_conversion_reference(const ReferenceConversionResult& value) {
    return value.value + 200;
}

int read_conversion_reference(const ReferenceConversionResult& value) {
    return value.value;
}

int read_conversion_reference_base(const ConversionReferenceBase& value) {
    return value.value;
}

long take_long(long value) {
    return value;
}

long take_long_reference(const long& value) {
    return value + 300;
}

long take_long_rvalue_reference(long&& value) {
    return value + 400;
}

double take_double_reference(const double& value) {
    return value + 500.0;
}

int choose_conversion(int value) {
    return value + 100;
}

int choose_constrained_conversion(int) {
    return 101;
}

int choose_constrained_conversion(double) {
    return 102;
}

long choose_conversion(long value) {
    return value + 200;
}

int choose_pointer_conversion(ConversionBasePointerTarget*) {
    return 71;
}

int choose_pointer_conversion(void*) {
    return 73;
}

int choose_pointer_to_bool_or_void(bool) {
    return 227;
}

int choose_pointer_to_bool_or_void(void*) {
    return 229;
}

int choose_pointer_void_qualification(void*) {
    return 271;
}

int choose_pointer_void_qualification(const void*) {
    return 277;
}

int take_pointer_conversion_bool(bool value) {
    return value ? 109 : 113;
}

int take_pointer_conversion_bool_reference(const bool& value) {
    return value ? 127 : 131;
}

int take_pointer_conversion_bool_rvalue_reference(bool&& value) {
    return value ? 137 : 139;
}

int take_member_pointer_conversion_bool(bool value) {
    return value ? 149 : 151;
}

int rank_member_function_signature(ConversionMemberFunctionInt) {
    return 167;
}

int rank_member_function_signature(ConversionMemberFunctionLong) {
    return 173;
}

int rank_member_function_noexcept(ConversionMemberFunctionInt) {
    return 179;
}

int rank_member_function_noexcept(ConversionMemberFunctionThrowing) {
    return 181;
}

int consume_void_pointer_reference(const void* const& value) {
    return value ? 157 : 163;
}

int consume_base_pointer_reference(
    const ConversionBasePointerTarget* const& value) {
    return value ? value->value + 100 : 167;
}

int choose_pointer_reference_conversion(
    const ConversionBasePointerTarget* const&) {
    return 197;
}

int choose_pointer_reference_conversion(const void* const&) {
    return 199;
}

template<typename T>
int choose_template_pointer_conversion(T, ConversionBasePointerTarget*) {
    return 79;
}

template<typename T>
int choose_template_pointer_conversion(T, void*) {
    return 83;
}

template<typename T>
int choose_template_pointer_reference_conversion(
    T, const ConversionBasePointerTarget* const&) {
    return 211;
}

template<typename T>
int choose_template_pointer_reference_conversion(T, const void* const&) {
    return 223;
}

template<typename T>
int choose_template_member_pointer_reference_conversion(
    T, int MemberPointerReferenceDerived::* const&) {
    return 239;
}

template<typename T>
int choose_template_member_pointer_reference_conversion(
    T, int MemberPointerReferenceLeaf::* const&) {
    return 241;
}

template<typename T>
int choose_template_member_pointer_cv_reference_conversion(
    T, int MemberPointerReferenceDerived::* const&) {
    return 243;
}

template<typename T>
int choose_template_member_pointer_cv_reference_conversion(
    T, const int MemberPointerReferenceDerived::* const&) {
    return 247;
}

template<typename T>
int choose_template_member_pointer_owner_cv_reference_conversion(
    T, const int MemberPointerReferenceDerived::* const&) {
    return 251;
}

template<typename T>
int choose_template_member_pointer_owner_cv_reference_conversion(
    T, int MemberPointerReferenceLeaf::* const&) {
    return 253;
}

template<typename T>
int choose_template_member_pointer_value_cv_conversion(
    T, int MemberPointerReferenceBase::* const& member) {
    return member ? 257 : 259;
}

template<typename T>
int choose_template_member_pointer_value_cv_conversion(
    T, const int MemberPointerReferenceBase::* const& member) {
    return member ? 263 : 265;
}

template<typename T>
int choose_template_nested_member_pointer_cv_conversion(
    T, MemberPointerNestedQualification const& member) {
    return member ? 269 : 271;
}

template<typename T>
int choose_template_nested_member_pointer_cv_conversion(
    T, MemberPointerNestedConstQualification const& member) {
    return member ? 277 : 279;
}

template<typename T>
int choose_template_member_pointer_array_cv_conversion(
    T, MemberPointerArray const& member) {
    return member ? 281 : 283;
}

template<typename T>
int choose_template_member_pointer_array_cv_conversion(
    T, ConstMemberPointerArray const& member) {
    return member ? 287 : 289;
}

template<typename T>
int take_template_integer(T, int value) {
    return value + 10;
}

template<typename T>
int choose_template_conversion(T, int value) {
    return value + 100;
}

template<typename T>
long choose_template_conversion(T, long value) {
    return value + 200;
}

int return_integer() {
    Meter meter{9};
    return meter;
}

long return_long() {
    Meter meter{11};
    return meter;
}

int main() {
    Meter meter{7};
    RefQualifiedConversionSource ref_qualified_source;
    const CvRefQualifiedConversionSource const_ref_qualified_source{};
    FunctionReferenceConversionSource function_reference_source;
    FunctionDesignatorToBoolSource function_designator_bool_source;
    FunctionReferenceConversionTarget& lvalue_function =
        function_reference_source;
    FunctionReferenceConversionTarget&& rvalue_function =
        function_reference_source;
    RvalueFunctionReferenceSource rvalue_function_reference_source;
    LvalueFunctionReferenceSource lvalue_function_reference_source;
    FunctionReferenceConversionTarget& lvalue_function_from_rvalue =
        rvalue_function_reference_source;
    FunctionReferenceConversionTarget&& rvalue_function_from_lvalue =
        lvalue_function_reference_source;
    ConversionDerivedPointerTarget derived_pointer_target{67};
    ConversionPointerSource pointer_source{&derived_pointer_target};
    int conversion_array_values[2] = {29, 31};
    ArrayReferenceConversionSource array_reference_source{
        &conversion_array_values};
    int pointer_bool_value = 1;
    ConversionPointerToBoolSource pointer_bool_source{&pointer_bool_value};
    ConversionPointerToBoolSource null_pointer_bool_source{nullptr};
    ConversionMemberPointerToBoolSource member_pointer_bool_source;
    member_pointer_bool_source.value =
        &ConversionMemberPointerToBoolTarget::value;
    ConversionReferenceRankingSource reference_ranking_source;
    ConversionObjectVsResultSource object_vs_result_source;
    Flag flag{1};
    RankedMeter ranked_meter;
    PromotionMeter promotion_meter;
    FloatingPromotionMeter floating_promotion_meter;
    TemplatedConversionSource templated_source;
    ConstrainedTemplatedConversionSource constrained_templated_source;
    ScalarReferenceConversionSource scalar_reference_source{71};
    MemberPointerReferenceDerived member_pointer_reference_object;
    int MemberPointerReferenceBase::* member_pointer_reference_value =
        &MemberPointerReferenceBase::value;
    int MemberPointerReferenceBase::* null_member_pointer_reference_value =
        nullptr;
    MemberPointerReferenceSource member_pointer_reference_source{
        member_pointer_reference_value};
    MemberPointerReferenceSource null_member_pointer_reference_source{
        null_member_pointer_reference_value};
    reference_ranking_source.middle.value = 83;
    reference_ranking_source.leaf.value = 89;
    member_pointer_reference_object.padding = 197;
    member_pointer_reference_object.value = 211;
    int direct = meter;
    int templated_direct = templated_source;
    int argument = take_integer(meter);
    int templated_argument = take_integer(templated_source);
    long widened = take_long(meter);
    long reference_widened = take_long_reference(meter);
    long rvalue_reference_widened = take_long_rvalue_reference(meter);
    double reference_promoted = take_double_reference(floating_promotion_meter);

    if (!flag) return 1;
    if (direct != 7 || templated_direct != 67 ||
        templated_argument != 67 || argument != 7 || widened != 7 ||
        reference_widened != 307 || rvalue_reference_widened != 407 ||
        reference_promoted != 531.0 ||
        read_scalar_conversion_reference(scalar_reference_source) != 71 ||
        increment_scalar_conversion_reference(scalar_reference_source) != 72 ||
        scalar_reference_source.value != 72 ||
        read_member_pointer_conversion_reference(
            member_pointer_reference_object,
            member_pointer_reference_source) != 211 ||
        read_member_pointer_conversion_value(
            member_pointer_reference_source) != 211 ||
        read_derived_member_pointer_conversion_value(
            member_pointer_reference_object,
            member_pointer_reference_source) != 211 ||
        !is_null_member_pointer_conversion_reference(
            null_member_pointer_reference_source) ||
        choose_template_member_pointer_reference_conversion(
            0, member_pointer_reference_source) != 239 ||
        choose_template_member_pointer_cv_reference_conversion(
            0, member_pointer_reference_source) != 243 ||
        choose_template_member_pointer_owner_cv_reference_conversion(
            0, member_pointer_reference_source) != 251 ||
        choose_template_member_pointer_value_cv_conversion(
            0, member_pointer_reference_source) != 257 ||
        choose_template_nested_member_pointer_cv_conversion(
            0, MemberPointerNestedQualificationSource{}) != 269 ||
        choose_template_member_pointer_array_cv_conversion(
            0, MemberPointerArraySource{}) != 281 ||
        rank_member_function_signature(
            ConversionMemberFunctionSignatureSource{}) != 167 ||
        rank_member_function_noexcept(
            ConversionMemberFunctionSignatureSource{}) != 179 ||
        choose_constrained_conversion(constrained_templated_source) != 101 ||
        return_integer() != 9 || return_long() != 11 ||
        choose_conversion(meter) != 107 ||
        take_template_integer(0, meter) != 17 ||
        choose_template_conversion(0, meter) != 107 ||
        take_ranked_integer(ranked_meter) != 19 ||
        take_promoted_integer(promotion_meter) != 23 ||
        take_promoted_double(floating_promotion_meter) != 31.0 ||
        take_function_pointer(lvalue_function_reference_source) != 97 ||
        sum_array_decay(array_reference_source) != 60 ||
        choose_conversion_reference(ReferenceConversionSource{}) != 143 ||
        read_conversion_reference(ref_qualified_source) != 47 ||
        read_conversion_reference(RefQualifiedConversionSource{}) != 53 ||
        read_conversion_reference(const_ref_qualified_source) != 59 ||
        read_conversion_reference(CvRefQualifiedConversionSource{}) != 61 ||
        lvalue_function() != 97 || rvalue_function() != 101 ||
        lvalue_function_from_rvalue() != 101 ||
        rvalue_function_from_lvalue() != 97 ||
        read_conversion_reference_base(reference_ranking_source) != 83 ||
        take_long(object_vs_result_source) != 103 ||
        choose_pointer_conversion(pointer_source) != 71 ||
        choose_pointer_to_bool_or_void(pointer_source) != 229 ||
        choose_pointer_void_qualification(pointer_source) != 271 ||
        choose_template_pointer_conversion(0, pointer_source) != 79 ||
        take_pointer_conversion_bool(pointer_bool_source) != 109 ||
        take_pointer_conversion_bool(null_pointer_bool_source) != 113 ||
        take_pointer_conversion_bool_reference(pointer_bool_source) != 127 ||
        take_pointer_conversion_bool_rvalue_reference(
            null_pointer_bool_source) != 139 ||
        take_pointer_conversion_bool(function_designator_bool_source) != 109 ||
        take_pointer_conversion_bool_reference(
            function_designator_bool_source) != 127 ||
        take_member_pointer_conversion_bool(member_pointer_bool_source) != 149 ||
        consume_void_pointer_reference(pointer_bool_source) != 157 ||
        consume_void_pointer_reference(null_pointer_bool_source) != 163 ||
        consume_base_pointer_reference(pointer_source) != 167 ||
        choose_pointer_reference_conversion(pointer_source) != 197 ||
        choose_template_pointer_reference_conversion(0, pointer_source) !=
            211) {
        return 1;
    }
    return 0;
}
