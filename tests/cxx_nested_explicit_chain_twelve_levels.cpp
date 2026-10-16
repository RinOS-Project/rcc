template <typename OwnerValue>
struct TwelveLevelExplicitChainOwner {
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
                        template <typename SixthValue>
                        struct Sixth {
                            template <typename SeventhValue>
                            struct Seventh {
                                template <typename EighthValue>
                                struct Eighth {
                                    template <typename NinthValue>
                                    struct Ninth {
                                        template <typename TenthValue>
                                        struct Tenth {
                                            template <typename EleventhValue>
                                            struct Eleventh {
                                                template <typename InnerValue>
                                                struct Inner {
                                                    int result() const { return 1800; }
                                                };
                                            };
                                        };
                                    };
                                };
                            };
                        };
                    };
                };
            };
        };
    };
};

template <>
struct TwelveLevelExplicitChainOwner<int> {
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
                        template <typename SixthValue>
                        struct Sixth {
                            template <typename SeventhValue>
                            struct Seventh {
                                template <typename EighthValue>
                                struct Eighth {
                                    template <typename NinthValue>
                                    struct Ninth {
                                        template <typename TenthValue>
                                        struct Tenth {
                                            template <typename EleventhValue>
                                            struct Eleventh {
                                                template <typename InnerValue>
                                                struct Inner {
                                                    int result() const { return 1810; }
                                                };
                                            };
                                        };
                                    };
                                };
                            };
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
template <>
template <>
template <>
template <>
template <>
template <>
template <>
struct TwelveLevelExplicitChainOwner<int>::First<char>::Second<short>::
    Third<int>::Fourth<float>::Fifth<long>::Sixth<char>::Seventh<float>::
    Eighth<long>::Ninth<short>::Tenth<int>::Eleventh<unsigned char>::
    Inner<double> {
    int result() const { return 1820; }
};

int main() {
    TwelveLevelExplicitChainOwner<long>::First<char>::Second<short>::Third<int>
        ::Fourth<float>::Fifth<long>::Sixth<char>::Seventh<float>::Eighth<long>
        ::Ninth<short>::Tenth<int>::Eleventh<unsigned char>::Inner<double>
        primary_owner;
    TwelveLevelExplicitChainOwner<int>::First<long>::Second<short>::Third<int>
        ::Fourth<float>::Fifth<long>::Sixth<char>::Seventh<float>::Eighth<long>
        ::Ninth<short>::Tenth<int>::Eleventh<unsigned char>::Inner<double>
        explicit_owner;
    TwelveLevelExplicitChainOwner<int>::First<char>::Second<short>::Third<int>
        ::Fourth<float>::Fifth<long>::Sixth<char>::Seventh<float>::Eighth<long>
        ::Ninth<short>::Tenth<int>::Eleventh<unsigned char>::Inner<double>
        explicit_member;

    if (primary_owner.result() != 1800) return 1;
    if (explicit_owner.result() != 1810) return 2;
    if (explicit_member.result() != 1820) return 3;
    return 0;
}
