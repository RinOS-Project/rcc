template <typename OwnerValue>
struct TenLevelExplicitChainOwner {
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
                                        template <typename InnerValue>
                                        struct Inner {
                                            int result() const { return 1700; }
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
struct TenLevelExplicitChainOwner<int> {
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
                                        template <typename InnerValue>
                                        struct Inner {
                                            int result() const { return 1710; }
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
struct TenLevelExplicitChainOwner<int>::First<char>::Second<short>::
    Third<int>::Fourth<float>::Fifth<long>::Sixth<char>::Seventh<float>::
    Eighth<long>::Ninth<short>::Inner<double> {
    int result() const { return 1720; }
};

int main() {
    TenLevelExplicitChainOwner<long>::First<char>::Second<short>::Third<int>
        ::Fourth<float>::Fifth<long>::Sixth<char>::Seventh<float>::Eighth<long>
        ::Ninth<short>::Inner<double> primary_owner;
    TenLevelExplicitChainOwner<int>::First<long>::Second<short>::Third<int>
        ::Fourth<float>::Fifth<long>::Sixth<char>::Seventh<float>::Eighth<long>
        ::Ninth<short>::Inner<double> explicit_owner;
    TenLevelExplicitChainOwner<int>::First<char>::Second<short>::Third<int>
        ::Fourth<float>::Fifth<long>::Sixth<char>::Seventh<float>::Eighth<long>
        ::Ninth<short>::Inner<double> explicit_member;

    if (primary_owner.result() != 1700) return 1;
    if (explicit_owner.result() != 1710) return 2;
    if (explicit_member.result() != 1720) return 3;
    return 0;
}
