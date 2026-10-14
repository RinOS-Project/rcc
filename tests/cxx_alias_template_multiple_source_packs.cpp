template<int Left, int Right>
struct AliasValuePair {};

template<typename... Values>
struct AliasValueList {};

template<typename Left, typename Right>
struct AliasTypePair {};

template<typename... Values>
struct AliasTypeList {};

template<typename Type, int Value>
struct AliasTypeValuePair {};

template<typename... Values>
struct AliasMixedList {};

template<typename Value>
struct AliasTemplateOne {};

template<typename Value>
struct AliasTemplateTwo {};

template<typename... Values>
struct AliasTemplateList {};

template<int... OwnerValues>
struct AliasValuePackOwner {
    template<int... AliasValues>
    using Zip = AliasValueList<
        AliasValuePair<OwnerValues * 2, AliasValues + 1>...>;

    template<typename... AliasTypes>
    using MixedZip = AliasMixedList<
        AliasTypeValuePair<AliasTypes, OwnerValues>...>;
};

template<typename... OwnerTypes>
struct AliasTypePackOwner {
    template<typename... AliasTypes>
    using Zip = AliasTypeList<AliasTypePair<OwnerTypes, AliasTypes>...>;

    template<int... AliasValues>
    using MixedZip = AliasMixedList<
        AliasTypeValuePair<OwnerTypes, AliasValues>...>;

    template<template<typename> class... AliasTemplates>
    using TemplateZip =
        AliasTemplateList<AliasTemplates<OwnerTypes>...>;
};

template<template<typename> class... Templates>
using AliasTemplatePackList =
    AliasTemplateList<Templates<int>...>;

using ZippedAliasValues =
    AliasValuePackOwner<1, 2, 3>::Zip<4, 5, 6>;
using ExpectedAliasValues = AliasValueList<
    AliasValuePair<2, 5>, AliasValuePair<4, 6>, AliasValuePair<6, 7>>;
using ZippedAliasTypes =
    AliasTypePackOwner<int, char, long>::Zip<float, double, short>;
using ExpectedAliasTypes = AliasTypeList<
    AliasTypePair<int, float>, AliasTypePair<char, double>,
    AliasTypePair<long, short>>;
using ZippedAliasTypeTemplatePacks =
    AliasTypePackOwner<int, char, long>::TemplateZip<
        AliasTemplateOne, AliasTemplateTwo, AliasTemplateOne>;
using ExpectedAliasTypeTemplatePacks = AliasTemplateList<
    AliasTemplateOne<int>, AliasTemplateTwo<char>, AliasTemplateOne<long>>;
using ZippedAliasTypeValues =
    AliasTypePackOwner<int, char, long>::MixedZip<2, 4, 6>;
using ExpectedAliasTypeValues = AliasMixedList<
    AliasTypeValuePair<int, 2>, AliasTypeValuePair<char, 4>,
    AliasTypeValuePair<long, 6>>;
using ZippedAliasValueTypes =
    AliasValuePackOwner<3, 5, 7>::MixedZip<int, char, long>;
using ExpectedAliasValueTypes = AliasMixedList<
    AliasTypeValuePair<int, 3>, AliasTypeValuePair<char, 5>,
    AliasTypeValuePair<long, 7>>;
using ZippedAliasTemplatePacks =
    AliasTemplatePackList<AliasTemplateOne, AliasTemplateTwo>;
using ExpectedAliasTemplatePacks =
    AliasTemplateList<AliasTemplateOne<int>, AliasTemplateTwo<int>>;
using AliasRightShiftValue = AliasValuePair<(8 >> 1), 4>;

int check_zipped_alias_values(ExpectedAliasValues*) {
    return 0;
}

int check_zipped_alias_types(ExpectedAliasTypes*) {
    return 0;
}

int check_zipped_alias_type_template_packs(ExpectedAliasTypeTemplatePacks*) {
    return 0;
}

int check_zipped_alias_type_values(ExpectedAliasTypeValues*) {
    return 0;
}

int check_zipped_alias_value_types(ExpectedAliasValueTypes*) {
    return 0;
}

int check_zipped_alias_template_packs(ExpectedAliasTemplatePacks*) {
    return 0;
}

int check_alias_right_shift(AliasValuePair<4, 4>*) {
    return 0;
}

int main() {
    ZippedAliasValues values;
    ZippedAliasTypes types;
    ZippedAliasTypeTemplatePacks type_template_packs;
    ZippedAliasTypeValues type_values;
    ZippedAliasValueTypes value_types;
    ZippedAliasTemplatePacks template_packs;
    AliasRightShiftValue shifted;
    if (check_zipped_alias_values(&values) != 0) return 1;
    if (check_zipped_alias_types(&types) != 0) return 1;
    if (check_zipped_alias_type_template_packs(&type_template_packs) != 0)
        return 1;
    if (check_zipped_alias_type_values(&type_values) != 0) return 1;
    if (check_zipped_alias_value_types(&value_types) != 0) return 1;
    if (check_zipped_alias_template_packs(&template_packs) != 0) return 1;
    return check_alias_right_shift(&shifted);
}
