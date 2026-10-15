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

template <typename OwnerValue>
struct NestedMultiLevelExplicitOwner {
    template <typename MidValue>
    struct Mid {
        template <typename InnerValue>
        struct Inner {
            int result() const { return 400; }
        };
    };
};

template <>
struct NestedMultiLevelExplicitOwner<int> {
    template <typename MidValue>
    struct Mid {
        template <typename InnerValue>
        struct Inner {
            int result() const { return 410; }
        };
    };
};

template <>
template <>
struct NestedMultiLevelExplicitOwner<int>::Mid<char>::Inner<long> {
    int result() const { return 420; }
};

template <typename OwnerValue>
struct NestedThreeLevelExplicitOwner {
    template <typename FirstValue>
    struct First {
        template <typename MidValue>
        struct Mid {
            template <typename InnerValue>
            struct Inner {
                int result() const { return 500; }
            };
        };
    };
};

template <>
struct NestedThreeLevelExplicitOwner<int> {
    template <typename FirstValue>
    struct First {
        template <typename MidValue>
        struct Mid {
            template <typename InnerValue>
            struct Inner {
                int result() const { return 510; }
            };
        };
    };
};

template <>
template <>
template <>
struct NestedThreeLevelExplicitOwner<int>::First<char>::Mid<short>::Inner<long> {
    int result() const { return 520; }
};

template <typename OwnerValue>
struct NestedFourLevelExplicitOwner {
    template <typename FirstValue>
    struct First {
        template <typename SecondValue>
        struct Second {
            template <typename ThirdValue>
            struct Third {
                template <typename InnerValue>
                struct Inner {
                    int result() const { return 600; }
                };
            };
        };
    };
};

template <>
struct NestedFourLevelExplicitOwner<int> {
    template <typename FirstValue>
    struct First {
        template <typename SecondValue>
        struct Second {
            template <typename ThirdValue>
            struct Third {
                template <typename InnerValue>
                struct Inner {
                    int result() const { return 610; }
                };
            };
        };
    };
};

template <>
template <>
template <>
template <>
struct NestedFourLevelExplicitOwner<int>::First<char>::Second<short>::Third<long>::Inner<double> {
    int result() const { return 620; }
};

template <typename OwnerValue>
struct NestedFiveLevelExplicitOwner {
    template <typename FirstValue>
    struct First {
        template <typename SecondValue>
        struct Second {
            template <typename ThirdValue>
            struct Third {
                template <typename FourthValue>
                struct Fourth {
                    template <typename InnerValue>
                    struct Inner {
                        int result() const { return 1300; }
                    };
                };
            };
        };
    };
};

template <>
struct NestedFiveLevelExplicitOwner<int> {
    template <typename FirstValue>
    struct First {
        template <typename SecondValue>
        struct Second {
            template <typename ThirdValue>
            struct Third {
                template <typename FourthValue>
                struct Fourth {
                    template <typename InnerValue>
                    struct Inner {
                        int result() const { return 1310; }
                    };
                };
            };
        };
    };
};

template <>
template <>
template <>
template <>
template <>
struct NestedFiveLevelExplicitOwner<int>::First<char>::Second<short>::Third<int>::Fourth<float>::Inner<double> {
    int result() const { return 1320; }
};

template <typename OwnerValue>
struct NestedSixLevelExplicitOwner {
    template <typename FirstValue>
    struct First {
        template <typename SecondValue>
        struct Second {
            template <typename ThirdValue>
            struct Third {
                template <typename FourthValue>
                struct Fourth {
                    template <typename FifthValue>
                    struct Fifth {
                        template <typename InnerValue>
                        struct Inner {
                            int result() const { return 1440; }
                        };
                    };
                };
            };
        };
    };
};

template <>
struct NestedSixLevelExplicitOwner<int> {
    template <typename FirstValue>
    struct First {
        template <typename SecondValue>
        struct Second {
            template <typename ThirdValue>
            struct Third {
                template <typename FourthValue>
                struct Fourth {
                    template <typename FifthValue>
                    struct Fifth {
                        template <typename InnerValue>
                        struct Inner {
                            int result() const { return 1450; }
                        };
                    };
                };
            };
        };
    };
};

template <>
template <>
template <>
template <>
template <>
template <>
struct NestedSixLevelExplicitOwner<int>::First<char>::Second<short>::
    Third<int>::Fourth<float>::Fifth<long>::Inner<double> {
    int result() const { return 1460; }
};

template <typename OwnerValue>
struct NestedFiveLevelSpecializedIntermediateOwner {
    template <typename FirstValue>
    struct First {
        template <typename SecondValue>
        struct Second {
            template <typename ThirdValue>
            struct Third {
                template <typename FourthValue>
                struct Fourth {
                    template <typename InnerValue>
                    struct Inner {
                        int result() const { return 1400; }
                    };
                };
            };
        };
    };
};

template <>
struct NestedFiveLevelSpecializedIntermediateOwner<int> {
    template <typename FirstValue>
    struct First {
        template <typename SecondValue>
        struct Second {
            template <typename ThirdValue>
            struct Third {
                template <typename FourthValue>
                struct Fourth {
                    template <typename InnerValue>
                    struct Inner {
                        int result() const { return 1410; }
                    };
                };
            };
        };
    };
};

template <>
template <>
struct NestedFiveLevelSpecializedIntermediateOwner<int>::First<char> {
    template <typename SecondValue>
    struct Second {
        template <typename ThirdValue>
        struct Third {
            template <typename FourthValue>
            struct Fourth {
                template <typename InnerValue>
                struct Inner {
                    int result() const { return 1420; }
                };
            };
        };
    };
};

template <>
template <>
template <>
template <>
struct NestedFiveLevelSpecializedIntermediateOwner<int>::First<char>::Second<short>::Third<int>::Fourth<float>::Inner<double> {
    int result() const { return 1430; }
};

template <typename OwnerValue>
struct NestedMidSpecializedOwner {
    template <typename MidValue>
    struct Mid {
        template <typename InnerValue>
        struct Inner {
            int result() const { return 700; }
        };
    };
};

template <>
template <>
struct NestedMidSpecializedOwner<int>::Mid<char> {
    template <typename InnerValue>
    struct Inner {
        int result() const { return 710; }
    };
};

template <>
template <>
struct NestedMidSpecializedOwner<int>::Mid<char>::Inner<long> {
    int result() const { return 720; }
};

template <typename OwnerValue>
struct NestedPartialOwnerChain {
    template <typename MidValue>
    struct Mid {
        template <typename LeafValue>
        struct Leaf {
            int result() const { return 800; }
        };
    };
};

template <typename OwnerValue>
struct NestedPartialOwnerChain<OwnerValue*> {
    template <typename MidValue>
    struct Mid {
        template <typename LeafValue>
        struct Leaf {
            int result() const { return 810; }
        };
    };
};

template <>
template <>
template <>
struct NestedPartialOwnerChain<int*>::Mid<char>::Leaf<long> {
    int result() const { return 820; }
};

template <typename OwnerValue>
struct NestedPartialNestedOwnerChain {
    template <typename MidValue>
    struct Mid {
        template <typename LeafValue>
        struct Leaf {
            int result() const { return 900; }
        };
    };
};

template <typename OwnerValue>
struct NestedPartialNestedOwnerChain<OwnerValue*> {
    template <typename MidValue>
    struct Mid {
        template <typename LeafValue>
        struct Leaf {
            int result() const { return 910; }
        };
    };
};

template <typename OwnerValue>
template <typename MidValue>
struct NestedPartialNestedOwnerChain<OwnerValue*>::Mid<MidValue*> {
    template <typename LeafValue>
    struct Leaf {
        int result() const { return 915; }
    };
};

template <typename OwnerValue>
template <typename MidValue>
template <typename LeafValue>
struct NestedPartialNestedOwnerChain<OwnerValue*>::Mid<MidValue*>::Leaf<LeafValue*> {
    int result() const { return 920; }
};

template <typename OwnerValue>
struct NestedDeepPartialOwnerChain {
    template <typename MidValue>
    struct Mid {
        template <typename SubValue>
        struct Sub {
            template <typename LeafValue>
            struct Leaf {
                int result() const { return 1000; }
            };
        };
    };
};

template <typename OwnerValue>
struct NestedDeepPartialOwnerChain<OwnerValue*> {
    template <typename MidValue>
    struct Mid {
        template <typename SubValue>
        struct Sub {
            template <typename LeafValue>
            struct Leaf {
                int result() const { return 1010; }
            };
        };
    };
};

template <typename OwnerValue>
template <typename MidValue>
struct NestedDeepPartialOwnerChain<OwnerValue*>::Mid<MidValue*> {
    template <typename SubValue>
    struct Sub {
        template <typename LeafValue>
        struct Leaf {
            int result() const { return 1015; }
        };
    };
};

template <typename OwnerValue>
template <typename MidValue>
template <typename SubValue>
struct NestedDeepPartialOwnerChain<OwnerValue*>::Mid<MidValue*>::Sub<SubValue*> {
    template <typename LeafValue>
    struct Leaf {
        int result() const { return 1020; }
    };
};

template <typename OwnerValue>
template <typename MidValue>
template <typename SubValue>
template <typename LeafValue>
struct NestedDeepPartialOwnerChain<OwnerValue*>::Mid<MidValue*>::Sub<SubValue*>::Leaf<LeafValue*> {
    int result() const { return 1030; }
};

template <typename OwnerValue>
struct NestedFiveLevelPartialOwnerChain {
    template <typename FirstValue>
    struct First {
        template <typename SecondValue>
        struct Second {
            template <typename ThirdValue>
            struct Third {
                template <typename FourthValue>
                struct Fourth {
                    template <typename LeafValue>
                    struct Leaf {
                        int result() const { return 1100; }
                    };
                };
            };
        };
    };
};

template <typename OwnerValue>
struct NestedFiveLevelPartialOwnerChain<OwnerValue*> {
    template <typename FirstValue>
    struct First {
        template <typename SecondValue>
        struct Second {
            template <typename ThirdValue>
            struct Third {
                template <typename FourthValue>
                struct Fourth {
                    template <typename LeafValue>
                    struct Leaf {
                        int result() const { return 1110; }
                    };
                };
            };
        };
    };
};

template <typename OwnerValue>
template <typename FirstValue>
struct NestedFiveLevelPartialOwnerChain<OwnerValue*>::First<FirstValue*> {
    template <typename SecondValue>
    struct Second {
        template <typename ThirdValue>
        struct Third {
            template <typename FourthValue>
            struct Fourth {
                template <typename LeafValue>
                struct Leaf {
                    int result() const { return 1120; }
                };
            };
        };
    };
};

template <typename OwnerValue>
template <typename FirstValue>
template <typename SecondValue>
struct NestedFiveLevelPartialOwnerChain<OwnerValue*>::First<FirstValue*>::Second<SecondValue*> {
    template <typename ThirdValue>
    struct Third {
        template <typename FourthValue>
        struct Fourth {
            template <typename LeafValue>
            struct Leaf {
                int result() const { return 1130; }
            };
        };
    };
};

template <typename OwnerValue>
template <typename FirstValue>
template <typename SecondValue>
template <typename ThirdValue>
struct NestedFiveLevelPartialOwnerChain<OwnerValue*>::First<FirstValue*>::Second<SecondValue*>::Third<ThirdValue*> {
    template <typename FourthValue>
    struct Fourth {
        template <typename LeafValue>
        struct Leaf {
            int result() const { return 1140; }
        };
    };
};

template <typename OwnerValue>
template <typename FirstValue>
template <typename SecondValue>
template <typename ThirdValue>
template <typename FourthValue>
struct NestedFiveLevelPartialOwnerChain<OwnerValue*>::First<FirstValue*>::Second<SecondValue*>::Third<ThirdValue*>::Fourth<FourthValue*> {
    template <typename LeafValue>
    struct Leaf {
        int result() const { return 1150; }
    };
};

template <typename OwnerValue>
template <typename FirstValue>
template <typename SecondValue>
template <typename ThirdValue>
template <typename FourthValue>
template <typename LeafValue>
struct NestedFiveLevelPartialOwnerChain<OwnerValue*>::First<FirstValue*>::Second<SecondValue*>::Third<ThirdValue*>::Fourth<FourthValue*>::Leaf<LeafValue*> {
    int result() const { return 1160; }
};

template <typename OwnerValue, int OwnerTag>
struct NestedMixedPartialOwnerChain {
    template <typename MidValue, int MidTag>
    struct Mid {
        template <typename LeafValue, int LeafTag>
        struct Leaf {
            int result() const { return 1500; }
        };
    };
};

template <typename OwnerValue>
struct NestedMixedPartialOwnerChain<OwnerValue*, 7> {
    template <typename MidValue, int MidTag>
    struct Mid {
        template <typename LeafValue, int LeafTag>
        struct Leaf {
            int result() const { return 1510; }
        };
    };
};

template <typename OwnerValue>
template <typename MidValue>
struct NestedMixedPartialOwnerChain<OwnerValue*, 7>::Mid<MidValue*, 11> {
    template <typename LeafValue, int LeafTag>
    struct Leaf {
        int result() const { return 1520; }
    };
};

template <typename OwnerValue>
template <typename MidValue>
template <typename LeafValue>
struct NestedMixedPartialOwnerChain<OwnerValue*, 7>::Mid<MidValue*, 11>::
    Leaf<LeafValue*, 13> {
    int result() const { return 1530; }
};

template <typename OwnerValue>
struct NestedOwnerPartialOrdering {
    template <typename MidValue>
    struct Mid {
        template <typename LeafValue>
        struct Leaf {
            int result() const { return 1700; }
        };
    };
};

template <typename OwnerValue>
struct NestedOwnerPartialOrdering<const OwnerValue*> {
    template <typename MidValue>
    struct Mid {
        template <typename LeafValue>
        struct Leaf {
            int result() const { return 1710; }
        };
    };
};

template <typename OwnerValue>
template <typename MidValue>
struct NestedOwnerPartialOrdering<const OwnerValue*>::Mid<MidValue*> {
    template <typename LeafValue>
    struct Leaf {
        int result() const { return 1720; }
    };
};

template <typename OwnerValue>
struct NestedOwnerPartialOrdering<OwnerValue*> {
    template <typename MidValue>
    struct Mid {
        template <typename LeafValue>
        struct Leaf {
            int result() const { return 1730; }
        };
    };
};

template <typename OwnerValue>
template <typename MidValue>
struct NestedOwnerPartialOrdering<OwnerValue*>::Mid<MidValue*> {
    template <typename LeafValue>
    struct Leaf {
        int result() const { return 1740; }
    };
};

template <typename OwnerValue>
struct NestedArrayPartialOwnerChain {
    template <typename MidValue>
    struct Mid {
        template <typename LeafValue>
        struct Leaf {
            int result() const { return 1800; }
        };
    };
};

template <typename OwnerValue>
struct NestedArrayPartialOwnerChain<OwnerValue[3]> {
    template <typename MidValue>
    struct Mid {
        template <typename LeafValue>
        struct Leaf {
            int result() const { return 1810; }
        };
    };
};

template <typename OwnerValue>
template <typename MidValue>
struct NestedArrayPartialOwnerChain<OwnerValue[3]>::Mid<MidValue[2]> {
    template <typename LeafValue>
    struct Leaf {
        int result() const { return 1820; }
    };
};

template <typename OwnerValue>
template <typename MidValue>
template <typename LeafValue>
struct NestedArrayPartialOwnerChain<OwnerValue[3]>::Mid<MidValue[2]>::
    Leaf<LeafValue[1]> {
    int result() const { return 1830; }
};

template <typename OwnerValue>
struct NestedReferenceArrayPartialOwnerChain {
    template <typename MidValue>
    struct Mid {
        template <typename LeafValue>
        struct Leaf {
            int result() const { return 1900; }
        };
    };
};

template <typename OwnerValue>
struct NestedReferenceArrayPartialOwnerChain<OwnerValue (&)[3]> {
    template <typename MidValue>
    struct Mid {
        template <typename LeafValue>
        struct Leaf {
            int result() const { return 1910; }
        };
    };
};

template <typename OwnerValue>
template <typename MidValue>
struct NestedReferenceArrayPartialOwnerChain<OwnerValue (&)[3]>::
    Mid<MidValue (&)[2]> {
    template <typename LeafValue>
    struct Leaf {
        int result() const { return 1920; }
    };
};

template <typename OwnerValue>
template <typename MidValue>
template <typename LeafValue>
struct NestedReferenceArrayPartialOwnerChain<OwnerValue (&)[3]>::
    Mid<MidValue (&)[2]>::Leaf<LeafValue (&)[1]> {
    int result() const { return 1930; }
};

template <typename OwnerValue>
struct NestedRvalueReferenceArrayPartialOwnerChain {
    template <typename MidValue>
    struct Mid {
        template <typename LeafValue>
        struct Leaf {
            int result() const { return 1940; }
        };
    };
};

template <typename OwnerValue>
struct NestedRvalueReferenceArrayPartialOwnerChain<OwnerValue (&&)[3]> {
    template <typename MidValue>
    struct Mid {
        template <typename LeafValue>
        struct Leaf {
            int result() const { return 1950; }
        };
    };
};

template <typename OwnerValue>
template <typename MidValue>
struct NestedRvalueReferenceArrayPartialOwnerChain<OwnerValue (&&)[3]>::
    Mid<MidValue (&&)[2]> {
    template <typename LeafValue>
    struct Leaf {
        int result() const { return 1960; }
    };
};

template <typename OwnerValue>
template <typename MidValue>
template <typename LeafValue>
struct NestedRvalueReferenceArrayPartialOwnerChain<OwnerValue (&&)[3]>::
    Mid<MidValue (&&)[2]>::Leaf<LeafValue (&&)[1]> {
    int result() const { return 1970; }
};

template <typename OwnerValue, int OwnerExtent>
struct NestedArrayBoundExpressionOwnerChain {
    template <typename MidValue, int MidExtent>
    struct Mid {
        template <typename LeafValue, int LeafExtent>
        struct Leaf {
            int result() const { return 2000; }
        };
    };
};

template <typename OwnerValue, int OwnerExtent>
struct NestedArrayBoundExpressionOwnerChain<
    OwnerValue[OwnerExtent + 1], OwnerExtent> {
    template <typename MidValue, int MidExtent>
    struct Mid {
        template <typename LeafValue, int LeafExtent>
        struct Leaf {
            int result() const { return 2010; }
        };
    };
};

template <typename OwnerValue, int OwnerExtent>
template <typename MidValue, int MidExtent>
struct NestedArrayBoundExpressionOwnerChain<
    OwnerValue[OwnerExtent + 1], OwnerExtent>::
    Mid<MidValue[MidExtent + 1], MidExtent> {
    template <typename LeafValue, int LeafExtent>
    struct Leaf {
        int result() const { return 2020; }
    };
};

template <typename OwnerValue, int OwnerExtent>
template <typename MidValue, int MidExtent>
template <typename LeafValue, int LeafExtent>
struct NestedArrayBoundExpressionOwnerChain<
    OwnerValue[OwnerExtent + 1], OwnerExtent>::
    Mid<MidValue[MidExtent + 1], MidExtent>::
    Leaf<LeafValue[LeafExtent + 1], LeafExtent> {
    int result() const { return 2030; }
};

template <typename OwnerValue, int OwnerTag>
struct NestedArrayDeducedExtentOwner {
    template <typename Value>
    struct Inner {
        int result() const { return 2200; }
    };
};

template <typename OwnerValue, int Extent>
struct NestedArrayDeducedExtentOwner<OwnerValue[Extent], 7> {
    template <typename Value>
    struct Inner {
        int result() const { return 2210; }
    };
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
    NestedMultiLevelExplicitOwner<long>::Mid<char>::Inner<long>
        multilevel_primary_outer;
    NestedMultiLevelExplicitOwner<int>::Mid<long>::Inner<long>
        multilevel_explicit_outer;
    NestedMultiLevelExplicitOwner<int>::Mid<char>::Inner<long>
        multilevel_explicit_member;
    NestedThreeLevelExplicitOwner<long>::First<char>::Mid<short>::Inner<long>
        three_level_primary_outer;
    NestedThreeLevelExplicitOwner<int>::First<long>::Mid<short>::Inner<long>
        three_level_explicit_outer;
    NestedThreeLevelExplicitOwner<int>::First<char>::Mid<short>::Inner<long>
        three_level_explicit_member;
    NestedFourLevelExplicitOwner<long>::First<char>::Second<short>::Third<long>::Inner<double>
        four_level_primary_outer;
    NestedFourLevelExplicitOwner<int>::First<long>::Second<short>::Third<long>::Inner<double>
        four_level_explicit_outer;
    NestedFourLevelExplicitOwner<int>::First<char>::Second<short>::Third<long>::Inner<double>
        four_level_explicit_member;
    NestedFiveLevelExplicitOwner<long>::First<char>::Second<short>::Third<int>
        ::Fourth<float>::Inner<double> five_level_explicit_primary_owner;
    NestedFiveLevelExplicitOwner<int>::First<long>::Second<short>::Third<int>
        ::Fourth<float>::Inner<double> five_level_explicit_owner;
    NestedFiveLevelExplicitOwner<int>::First<char>::Second<short>::Third<int>
        ::Fourth<float>::Inner<double> five_level_explicit_member;
    NestedSixLevelExplicitOwner<long>::First<char>::Second<short>::Third<int>
        ::Fourth<float>::Fifth<long>::Inner<double>
        six_level_explicit_primary_owner;
    NestedSixLevelExplicitOwner<int>::First<long>::Second<short>::Third<int>
        ::Fourth<float>::Fifth<long>::Inner<double> six_level_explicit_owner;
    NestedSixLevelExplicitOwner<int>::First<char>::Second<short>::Third<int>
        ::Fourth<float>::Fifth<long>::Inner<double>
        six_level_explicit_member;
    NestedFiveLevelSpecializedIntermediateOwner<long>::First<char>
        ::Second<short>::Third<int>::Fourth<float>::Inner<double>
        five_level_specialized_primary_owner;
    NestedFiveLevelSpecializedIntermediateOwner<int>::First<long>
        ::Second<short>::Third<int>::Fourth<float>::Inner<double>
        five_level_specialized_outer_owner;
    NestedFiveLevelSpecializedIntermediateOwner<int>::First<char>
        ::Second<long>::Third<int>::Fourth<float>::Inner<double>
        five_level_specialized_intermediate_owner;
    NestedFiveLevelSpecializedIntermediateOwner<int>::First<char>
        ::Second<short>::Third<int>::Fourth<float>::Inner<double>
        five_level_specialized_deep_member;
    NestedMidSpecializedOwner<long>::Mid<char>::Inner<long>
        specialized_mid_primary_owner;
    NestedMidSpecializedOwner<int>::Mid<char>::Inner<char>
        specialized_mid_member_template;
    NestedMidSpecializedOwner<int>::Mid<char>::Inner<long>
        specialized_mid_explicit_member;
    NestedPartialOwnerChain<long>::Mid<char>::Leaf<long>
        partial_chain_primary_owner;
    NestedPartialOwnerChain<int*>::Mid<long>::Leaf<long>
        partial_chain_unspecialized_mid;
    NestedPartialOwnerChain<int*>::Mid<char>::Leaf<char>
        partial_chain_unspecialized_leaf;
    NestedPartialOwnerChain<int*>::Mid<char>::Leaf<long>
        partial_chain_explicit_leaf;
    NestedPartialNestedOwnerChain<long>::Mid<char*>::Leaf<int*>
        nested_partial_chain_primary_owner;
    NestedPartialNestedOwnerChain<int*>::Mid<char>::Leaf<int*>
        nested_partial_chain_outer_owner;
    NestedPartialNestedOwnerChain<int*>::Mid<char*>::Leaf<int>
        nested_partial_chain_mid_specialization;
    NestedPartialNestedOwnerChain<int*>::Mid<char*>::Leaf<int*>
        nested_partial_chain_leaf_specialization;
    NestedDeepPartialOwnerChain<long>::Mid<char*>::Sub<short*>::Leaf<int*>
        deep_partial_chain_primary_owner;
    NestedDeepPartialOwnerChain<int*>::Mid<char>::Sub<short*>::Leaf<int*>
        deep_partial_chain_outer_owner;
    NestedDeepPartialOwnerChain<int*>::Mid<char*>::Sub<short>::Leaf<int*>
        deep_partial_chain_mid_owner;
    NestedDeepPartialOwnerChain<int*>::Mid<char*>::Sub<short*>::Leaf<int>
        deep_partial_chain_sub_owner;
    NestedDeepPartialOwnerChain<int*>::Mid<char*>::Sub<short*>::Leaf<int*>
        deep_partial_chain_leaf_owner;
    NestedFiveLevelPartialOwnerChain<long>::First<char>::Second<short>
        ::Third<int>::Fourth<float>::Leaf<double> five_level_primary_owner;
    NestedFiveLevelPartialOwnerChain<int*>::First<char>::Second<short>
        ::Third<int>::Fourth<float>::Leaf<double> five_level_outer_owner;
    NestedFiveLevelPartialOwnerChain<int*>::First<char*>::Second<short>
        ::Third<int>::Fourth<float>::Leaf<double> five_level_first_owner;
    NestedFiveLevelPartialOwnerChain<int*>::First<char*>::Second<short*>
        ::Third<int>::Fourth<float>::Leaf<double> five_level_second_owner;
    NestedFiveLevelPartialOwnerChain<int*>::First<char*>::Second<short*>
        ::Third<int*>::Fourth<float>::Leaf<double> five_level_third_owner;
    NestedFiveLevelPartialOwnerChain<int*>::First<char*>::Second<short*>
        ::Third<int*>::Fourth<float*>::Leaf<double> five_level_fourth_owner;
    NestedFiveLevelPartialOwnerChain<int*>::First<char*>::Second<short*>
        ::Third<int*>::Fourth<float*>::Leaf<double*> five_level_leaf_owner;
    NestedMixedPartialOwnerChain<long, 6>::Mid<char, 10>::Leaf<int, 12>
        mixed_partial_primary_owner;
    NestedMixedPartialOwnerChain<int*, 7>::Mid<char, 10>::Leaf<int, 12>
        mixed_partial_outer_owner;
    NestedMixedPartialOwnerChain<int*, 7>::Mid<char*, 11>::Leaf<int, 12>
        mixed_partial_mid_owner;
    NestedMixedPartialOwnerChain<int*, 7>::Mid<char*, 11>::Leaf<int*, 13>
        mixed_partial_leaf_owner;
    NestedOwnerPartialOrdering<long>::Mid<char>::Leaf<long>
        partial_ordering_primary_owner;
    NestedOwnerPartialOrdering<const int*>::Mid<char>::Leaf<long>
        partial_ordering_const_owner;
    NestedOwnerPartialOrdering<const int*>::Mid<char*>::Leaf<long>
        partial_ordering_const_mid_owner;
    NestedOwnerPartialOrdering<int*>::Mid<char>::Leaf<long>
        partial_ordering_pointer_owner;
    NestedOwnerPartialOrdering<int*>::Mid<char*>::Leaf<long>
        partial_ordering_pointer_mid_owner;
    NestedArrayPartialOwnerChain<long>::Mid<char>::Leaf<int>
        array_partial_primary_owner;
    NestedArrayPartialOwnerChain<int[3]>::Mid<char>::Leaf<int>
        array_partial_outer_owner;
    NestedArrayPartialOwnerChain<int[3]>::Mid<char[2]>::Leaf<int>
        array_partial_mid_owner;
    NestedArrayPartialOwnerChain<int[3]>::Mid<char[2]>::Leaf<long[1]>
        array_partial_leaf_owner;
    NestedReferenceArrayPartialOwnerChain<long (&)[4]>::Mid<char>::Leaf<int>
        reference_array_partial_primary_owner;
    NestedReferenceArrayPartialOwnerChain<int (&)[3]>::Mid<char>::Leaf<int>
        reference_array_partial_outer_owner;
    NestedReferenceArrayPartialOwnerChain<int (&)[3]>::Mid<char (&)[2]>::
        Leaf<int> reference_array_partial_mid_owner;
    NestedReferenceArrayPartialOwnerChain<int (&)[3]>::Mid<char (&)[2]>::
        Leaf<long (&)[1]> reference_array_partial_leaf_owner;
    NestedRvalueReferenceArrayPartialOwnerChain<long (&&)[4]>::Mid<char>::
        Leaf<int> rvalue_reference_array_partial_primary_owner;
    NestedRvalueReferenceArrayPartialOwnerChain<int (&&)[3]>::Mid<char>::
        Leaf<int> rvalue_reference_array_partial_outer_owner;
    NestedRvalueReferenceArrayPartialOwnerChain<int (&&)[3]>::
        Mid<char (&&)[2]>::Leaf<int> rvalue_reference_array_partial_mid_owner;
    NestedRvalueReferenceArrayPartialOwnerChain<int (&&)[3]>::
        Mid<char (&&)[2]>::Leaf<long (&&)[1]>
        rvalue_reference_array_partial_leaf_owner;
    NestedArrayBoundExpressionOwnerChain<int[5], 3>::
        Mid<char[3], 2>::Leaf<long[2], 1> array_bound_expression_primary;
    NestedArrayBoundExpressionOwnerChain<int[4], 3>::
        Mid<char[3], 2>::Leaf<long[2], 1> array_bound_expression_outer;
    NestedArrayBoundExpressionOwnerChain<int[4], 3>::
        Mid<char[4], 2>::Leaf<long[2], 1>
        array_bound_expression_wrong_mid_extent;
    NestedArrayBoundExpressionOwnerChain<int[4], 3>::
        Mid<char[3], 2>::Leaf<long[3], 1>
        array_bound_expression_wrong_leaf_extent;
    NestedArrayDeducedExtentOwner<int[4], 7>::Inner<char>
        array_bound_deduced_extent_partial;
    NestedArrayDeducedExtentOwner<int[4], 8>::Inner<char>
        array_bound_deduced_extent_primary;
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
    if (multilevel_primary_outer.result() != 400) return 27;
    if (multilevel_explicit_outer.result() != 410) return 28;
    if (multilevel_explicit_member.result() != 420) return 29;
    if (three_level_primary_outer.result() != 500) return 30;
    if (three_level_explicit_outer.result() != 510) return 31;
    if (three_level_explicit_member.result() != 520) return 32;
    if (four_level_primary_outer.result() != 600) return 33;
    if (four_level_explicit_outer.result() != 610) return 34;
    if (four_level_explicit_member.result() != 620) return 35;
    if (five_level_explicit_primary_owner.result() != 1300) return 59;
    if (five_level_explicit_owner.result() != 1310) return 60;
    if (five_level_explicit_member.result() != 1320) return 61;
    if (six_level_explicit_primary_owner.result() != 1440) return 87;
    if (six_level_explicit_owner.result() != 1450) return 88;
    if (six_level_explicit_member.result() != 1460) return 89;
    if (five_level_specialized_primary_owner.result() != 1400) return 62;
    if (five_level_specialized_outer_owner.result() != 1410) return 63;
    if (five_level_specialized_intermediate_owner.result() != 1420) return 64;
    if (five_level_specialized_deep_member.result() != 1430) return 65;
    if (specialized_mid_primary_owner.result() != 700) return 36;
    if (specialized_mid_member_template.result() != 710) return 37;
    if (specialized_mid_explicit_member.result() != 720) return 38;
    if (partial_chain_primary_owner.result() != 800) return 39;
    if (partial_chain_unspecialized_mid.result() != 810) return 40;
    if (partial_chain_unspecialized_leaf.result() != 810) return 41;
    if (partial_chain_explicit_leaf.result() != 820) return 42;
    if (nested_partial_chain_primary_owner.result() != 900) return 43;
    if (nested_partial_chain_outer_owner.result() != 910) return 44;
    if (nested_partial_chain_mid_specialization.result() != 915) return 45;
    if (nested_partial_chain_leaf_specialization.result() != 920) return 46;
    if (deep_partial_chain_primary_owner.result() != 1000) return 47;
    if (deep_partial_chain_outer_owner.result() != 1010) return 48;
    if (deep_partial_chain_mid_owner.result() != 1015) return 49;
    if (deep_partial_chain_sub_owner.result() != 1020) return 50;
    if (deep_partial_chain_leaf_owner.result() != 1030) return 51;
    if (five_level_primary_owner.result() != 1100) return 52;
    if (five_level_outer_owner.result() != 1110) return 53;
    if (five_level_first_owner.result() != 1120) return 54;
    if (five_level_second_owner.result() != 1130) return 55;
    if (five_level_third_owner.result() != 1140) return 56;
    if (five_level_fourth_owner.result() != 1150) return 57;
    if (five_level_leaf_owner.result() != 1160) return 58;
    if (mixed_partial_primary_owner.result() != 1500) return 66;
    if (mixed_partial_outer_owner.result() != 1510) return 67;
    if (mixed_partial_mid_owner.result() != 1520) return 68;
    if (mixed_partial_leaf_owner.result() != 1530) return 69;
    if (partial_ordering_primary_owner.result() != 1700) return 70;
    if (partial_ordering_const_owner.result() != 1710) return 71;
    if (partial_ordering_const_mid_owner.result() != 1720) return 72;
    if (partial_ordering_pointer_owner.result() != 1730) return 73;
    if (partial_ordering_pointer_mid_owner.result() != 1740) return 74;
    if (array_partial_primary_owner.result() != 1800) return 75;
    if (array_partial_outer_owner.result() != 1810) return 76;
    if (array_partial_mid_owner.result() != 1820) return 77;
    if (array_partial_leaf_owner.result() != 1830) return 78;
    if (reference_array_partial_primary_owner.result() != 1900) return 79;
    if (reference_array_partial_outer_owner.result() != 1910) return 80;
    if (reference_array_partial_mid_owner.result() != 1920) return 81;
    if (reference_array_partial_leaf_owner.result() != 1930) return 82;
    if (rvalue_reference_array_partial_primary_owner.result() != 1940)
        return 83;
    if (rvalue_reference_array_partial_outer_owner.result() != 1950)
        return 84;
    if (rvalue_reference_array_partial_mid_owner.result() != 1960)
        return 85;
    if (rvalue_reference_array_partial_leaf_owner.result() != 1970)
        return 86;
    if (array_bound_expression_primary.result() != 2000) return 90;
    if (array_bound_expression_outer.result() != 2030) return 91;
    if (array_bound_expression_wrong_mid_extent.result() != 2010) return 92;
    if (array_bound_expression_wrong_leaf_extent.result() != 2020) return 93;
    if (array_bound_deduced_extent_partial.result() != 2210) return 94;
    if (array_bound_deduced_extent_primary.result() != 2200) return 95;
    return 0;
}
