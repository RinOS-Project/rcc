template <typename OwnerValue>
struct ThirteenLevelExplicitChainOwner {
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
                                                template <typename TwelfthValue>
                                                struct Twelfth {
                                                    template <typename InnerValue>
                                                    struct Inner {
                                                        int result() const {
                                                            return 1900;
                                                        }
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
};

template <>
struct ThirteenLevelExplicitChainOwner<int> {
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
                                                template <typename TwelfthValue>
                                                struct Twelfth {
                                                    template <typename InnerValue>
                                                    struct Inner {
                                                        int result() const {
                                                            return 1910;
                                                        }
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
template <>
struct ThirteenLevelExplicitChainOwner<int>::First<char>::Second<short>::
    Third<int>::Fourth<float>::Fifth<long>::Sixth<char>::Seventh<float>::
    Eighth<long>::Ninth<short>::Tenth<int>::Eleventh<unsigned char>::
    Twelfth<long>::Inner<double> {
    int result() const { return 1920; }
};

int main() {
    ThirteenLevelExplicitChainOwner<long>::First<char>::Second<short>::
        Third<int>::Fourth<float>::Fifth<long>::Sixth<char>::Seventh<float>::
        Eighth<long>::Ninth<short>::Tenth<int>::Eleventh<unsigned char>::
        Twelfth<long>::Inner<double> primary_owner;
    ThirteenLevelExplicitChainOwner<int>::First<long>::Second<short>::
        Third<int>::Fourth<float>::Fifth<long>::Sixth<char>::Seventh<float>::
        Eighth<long>::Ninth<short>::Tenth<int>::Eleventh<unsigned char>::
        Twelfth<long>::Inner<double> explicit_owner;
    ThirteenLevelExplicitChainOwner<int>::First<char>::Second<short>::
        Third<int>::Fourth<float>::Fifth<long>::Sixth<char>::Seventh<float>::
        Eighth<long>::Ninth<short>::Tenth<int>::Eleventh<unsigned char>::
        Twelfth<long>::Inner<double> explicit_member;

    if (primary_owner.result() != 1900) return 1;
    if (explicit_owner.result() != 1910) return 2;
    if (explicit_member.result() != 1920) return 3;
    return 0;
}
