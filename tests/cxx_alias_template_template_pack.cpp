template<typename T>
struct PackBox {
    T value;
};

template<typename T>
using PackBoxAlias = PackBox<T>;

template<int Count>
struct NumberBox {
    unsigned char values[Count];
};

template<int Count>
using NumberBoxAlias = NumberBox<Count>;

template<typename... Types>
struct PackList {
    unsigned char values[sizeof...(Types) + 1];
};

template<template<typename> class Template = PackBoxAlias>
struct AliasDefaultHolder {
    Template<int> value;
};

template<template<typename> class Template>
struct AliasArgumentHolder {
    Template<int> value;

    int get() const {
        return value.value;
    }
};

template<template<typename> class Template>
int read_alias_argument(Template<int>* value) {
    return value->value;
}

template<template<typename> class... Templates>
using ReboundPackList = PackList<Templates<int>...>;

template<template<int> class... Templates>
using ReboundNumberPackList = PackList<Templates<3>...>;

using EmptyPack = ReboundPackList<>;
using AliasOnlyPack = ReboundPackList<PackBoxAlias>;
using MixedPack = ReboundPackList<PackBox, PackBoxAlias>;
using NonTypeAliasPack = ReboundNumberPackList<NumberBoxAlias>;
using DefaultAliasHolder = AliasDefaultHolder<>;

int main() {
    AliasArgumentHolder<PackBoxAlias> holder;
    PackBox<int> box = {17};
    DefaultAliasHolder default_holder;
    holder.value.value = 23;
    default_holder.value.value = 19;
    return sizeof(EmptyPack) == sizeof(PackList<>) &&
                   sizeof(AliasOnlyPack) == sizeof(PackList<PackBox<int>>) &&
                   sizeof(MixedPack) ==
                       sizeof(PackList<PackBox<int>, PackBoxAlias<int>>) &&
                   sizeof(NonTypeAliasPack) ==
                       sizeof(PackList<NumberBox<3>>) &&
                   holder.get() == 23 &&
                   read_alias_argument<PackBoxAlias>(&box) == 17 &&
                   default_holder.value.value == 19
        ? 0 : 1;
}
