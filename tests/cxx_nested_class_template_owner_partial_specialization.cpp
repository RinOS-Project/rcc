template <typename OwnerValue>
struct NestedPartialTemplateOwner {
    template <typename Value>
    struct Inner {
        int result() const { return 10; }
    };
};

template <typename OwnerValue>
template <typename Value>
struct NestedPartialTemplateOwner<OwnerValue>::Inner<Value*> {
    int result() const { return 20; }
};

template <typename... OwnerValues>
struct NestedPartialTemplateOwnerPack {
    template <typename Value>
    struct Inner {
        int result() const { return 30; }
    };
};

template <typename... OwnerValues>
template <typename Value>
struct NestedPartialTemplateOwnerPack<OwnerValues...>::Inner<Value*> {
    int result() const { return 40; }
};

template <int... OwnerValues>
struct NestedPartialValueTemplateOwnerPack {
    template <typename Value>
    struct Inner {
        int result() const { return 50; }
    };
};

template <int... OwnerValues>
template <typename Value>
struct NestedPartialValueTemplateOwnerPack<OwnerValues...>::Inner<Value*> {
    int result() const { return 60; }
};

template <typename Value>
struct NestedPartialOwnerBox {};

template <template <typename> class... OwnerTemplates>
struct NestedPartialTemplateTemplateOwnerPack {
    template <typename Value>
    struct Inner {
        int result() const { return 70; }
    };
};

template <template <typename> class... OwnerTemplates>
template <typename Value>
struct NestedPartialTemplateTemplateOwnerPack<OwnerTemplates...>::Inner<Value*> {
    int result() const { return 80; }
};

template <typename OwnerValue>
struct NestedPartialSpecializedOuterOwner {
    template <typename Value>
    struct Inner {
        int result() const { return 90; }
    };
};

template <typename OwnerValue>
struct NestedPartialSpecializedOuterOwner<OwnerValue*> {
    template <typename Value>
    struct Inner {
        int result() const { return 100; }
    };
};

template <typename OwnerValue>
template <typename Value>
struct NestedPartialSpecializedOuterOwner<OwnerValue*>::Inner<Value*> {
    int result() const { return 110; }
    int owner_size() const { return sizeof(OwnerValue); }
};

template <int OwnerCount, typename OwnerValue>
struct NestedPartialValueSpecializedOuterOwner {
    template <typename Value>
    struct Inner {
        int result() const { return 120; }
    };
};

template <int OwnerCount, typename OwnerValue>
struct NestedPartialValueSpecializedOuterOwner<OwnerCount, OwnerValue*> {
    template <typename Value>
    struct Inner {
        int result() const { return 125 + OwnerCount; }
    };
};

template <int OwnerCount, typename OwnerValue>
template <typename Value>
struct NestedPartialValueSpecializedOuterOwner<OwnerCount, OwnerValue*>::
    Inner<Value*> {
    int result() const { return 130 + OwnerCount + sizeof(OwnerValue); }
};

template <typename Element>
struct NestedPartialOwnerTemplateBox {
    using payload = Element;
};

template <template <typename> class OwnerTemplate, typename OwnerValue>
struct NestedPartialTemplateSpecializedOuterOwner {
    template <typename Value>
    struct Inner {
        int result() const { return 140; }
    };
};

template <template <typename> class OwnerTemplate, typename OwnerValue>
struct NestedPartialTemplateSpecializedOuterOwner<OwnerTemplate, OwnerValue*> {
    template <typename Value>
    struct Inner {
        int result() const { return 160; }
    };
};

template <template <typename> class OwnerTemplate, typename OwnerValue>
template <typename Value>
struct NestedPartialTemplateSpecializedOuterOwner<OwnerTemplate, OwnerValue*>::
    Inner<Value*> {
    int result() const { return 170 + sizeof(OwnerValue); }
};

template <decltype(nullptr) OwnerNull, typename OwnerValue>
struct NestedNullptrSpecializedOuterOwner {
    template <typename Value>
    struct Inner {
        int result() const { return 180; }
    };
};

template <typename OwnerValue>
struct NestedNullptrSpecializedOuterOwner<nullptr, OwnerValue> {
    template <typename Value>
    struct Inner {
        int result() const { return 185; }
    };
};

template <typename OwnerValue>
template <typename Value>
struct NestedNullptrSpecializedOuterOwner<nullptr, OwnerValue>::
    Inner<Value*> {
    int result() const { return 190 + sizeof(OwnerValue); }
};

template <typename OuterType, int FirstValue, int LastValue>
struct NestedExpressionSpecializedOuterOwner {
    template <typename Value>
    struct Inner {
        int result() const { return 200; }
    };
};

template <typename OwnerType, int OwnerValue>
struct NestedExpressionSpecializedOuterOwner<
    OwnerType*, OwnerValue, OwnerValue * 3 + 2> {
    template <typename Value>
    struct Inner {
        int result() const { return 205; }
    };
};

template <typename OwnerType, int OwnerValue>
template <typename Value>
struct NestedExpressionSpecializedOuterOwner<
    OwnerType*, OwnerValue, OwnerValue * 3 + 2>::Inner<Value*> {
    int result() const { return 210 + sizeof(OwnerType) + OwnerValue; }
};

template <typename First, typename Second>
struct NestedReorderedTemplateOwner {
    template <typename Value>
    struct Inner {
        int result() const { return 220; }
    };
};

template <typename Left, typename Right>
struct NestedReorderedTemplateOwner<Right*, Left> {
    template <typename Value>
    struct Inner {
        int result() const { return 230; }
    };
};

template <typename Left, typename Right>
template <typename Value>
struct NestedReorderedTemplateOwner<Right*, Left>::Inner<Value*> {
    int result() const { return 240 + sizeof(Left) + sizeof(Right); }
};

template <typename OwnerValue>
struct NestedExplicitSpecializedOuterOwner {
    template <typename Value>
    struct Inner {
        int result() const { return 250; }
    };
};

template <typename OwnerValue>
struct NestedExplicitSpecializedOuterOwner<OwnerValue*> {
    template <typename Value>
    struct Inner {
        int result() const { return 260; }
    };
};

template <>
template <>
struct NestedExplicitSpecializedOuterOwner<int*>::Inner<char> {
    int result() const { return 270; }
};

struct NestedConcreteOwnerExplicitSpecialization {
    template <typename Value>
    struct Inner {
        int result() const { return 280; }
    };
};

template <>
struct NestedConcreteOwnerExplicitSpecialization::Inner<char> {
    int result() const { return 290; }
};

template <typename OwnerValue>
struct NestedExplicitMemberOfExplicitOuterOwner {
    template <typename Value>
    struct Inner {
        int result() const { return 300; }
    };
};

template <>
struct NestedExplicitMemberOfExplicitOuterOwner<int> {
    template <typename Value>
    struct Inner {
        int result() const { return 310; }
    };
};

template <>
struct NestedExplicitMemberOfExplicitOuterOwner<int>::Inner<char> {
    int result() const { return 320; }
};

int main() {
    NestedPartialTemplateOwner<int>::Inner<char> primary_int;
    NestedPartialTemplateOwner<long>::Inner<char*> partial_long;
    NestedPartialTemplateOwner<unsigned>::Inner<void*> partial_unsigned;
    NestedPartialTemplateOwnerPack<>::Inner<char*> empty_owner_pack;
    NestedPartialTemplateOwnerPack<int, long>::Inner<void*> type_owner_pack;
    NestedPartialValueTemplateOwnerPack<>::Inner<char*> empty_value_pack;
    NestedPartialValueTemplateOwnerPack<2, 4>::Inner<double*> value_owner_pack;
    NestedPartialTemplateTemplateOwnerPack<
        NestedPartialOwnerBox>::Inner<void*> template_owner_pack;
    NestedPartialSpecializedOuterOwner<int>::Inner<char*> primary_outer_partial;
    NestedPartialSpecializedOuterOwner<int*>::Inner<char> partial_outer_primary;
    NestedPartialSpecializedOuterOwner<int*>::Inner<char*> partial_outer_partial;
    NestedPartialValueSpecializedOuterOwner<3, int*>::Inner<char*>
        partial_outer_nontype;
    NestedPartialTemplateSpecializedOuterOwner<
        NestedPartialOwnerTemplateBox, int*>::Inner<char*>
        partial_outer_template_template;
    NestedNullptrSpecializedOuterOwner<nullptr, int>::Inner<char*>
        partial_outer_nullptr;
    NestedExpressionSpecializedOuterOwner<int*, 1, 5>::Inner<char*>
        partial_outer_expression;
    NestedExpressionSpecializedOuterOwner<int*, 1, 4>::Inner<char*>
        primary_outer_expression;
    NestedReorderedTemplateOwner<int*, char>::Inner<long*>
        reordered_outer_partial;
    NestedReorderedTemplateOwner<int, char*>::Inner<long*>
        primary_reordered_outer;
    NestedExplicitSpecializedOuterOwner<long>::Inner<char>
        explicit_outer_primary;
    NestedExplicitSpecializedOuterOwner<int*>::Inner<long>
        explicit_outer_partial_primary;
    NestedExplicitSpecializedOuterOwner<int*>::Inner<char>
        explicit_nested_member;
    NestedConcreteOwnerExplicitSpecialization::Inner<char>
        explicit_concrete_owner_member;
    NestedExplicitMemberOfExplicitOuterOwner<long>::Inner<char>
        explicit_outer_owner_primary;
    NestedExplicitMemberOfExplicitOuterOwner<int>::Inner<long>
        explicit_outer_owner_member_primary;
    NestedExplicitMemberOfExplicitOuterOwner<int>::Inner<char>
        explicit_outer_owner_member_specialized;
    if (primary_int.result() != 10) return 1;
    if (partial_long.result() != 20) return 2;
    if (partial_unsigned.result() != 20) return 3;
    if (empty_owner_pack.result() != 40) return 4;
    if (type_owner_pack.result() != 40) return 5;
    if (empty_value_pack.result() != 60) return 6;
    if (value_owner_pack.result() != 60) return 7;
    if (template_owner_pack.result() != 80) return 8;
    if (primary_outer_partial.result() != 90) return 9;
    if (partial_outer_primary.result() != 100) return 10;
    if (partial_outer_partial.result() != 110) return 11;
    if (partial_outer_partial.owner_size() != sizeof(int)) return 12;
    if (partial_outer_nontype.result() != 137) return 13;
    if (partial_outer_template_template.result() != 174) return 14;
    if (partial_outer_nullptr.result() != 194) return 15;
    if (partial_outer_expression.result() != 215) return 16;
    if (primary_outer_expression.result() != 200) return 17;
    if (reordered_outer_partial.result() != 245) return 18;
    if (primary_reordered_outer.result() != 220) return 19;
    if (explicit_outer_primary.result() != 250) return 20;
    if (explicit_outer_partial_primary.result() != 260) return 21;
    if (explicit_nested_member.result() != 270) return 22;
    if (explicit_concrete_owner_member.result() != 290) return 23;
    if (explicit_outer_owner_primary.result() != 300) return 24;
    if (explicit_outer_owner_member_primary.result() != 310) return 25;
    if (explicit_outer_owner_member_specialized.result() != 320) return 26;
    return 0;
}
