class FriendHost {
    int value;

public:
    friend int inline_friend(FriendHost& host, int replacement) {
        host.value = replacement;
        return host.value + 1;
    }

    friend int declared_friend(FriendHost& host, int replacement);
};

namespace associated_adl {
namespace nested {
struct Argument {
    int value;
};

int find_in_associated_namespace(Argument& argument) {
    return argument.value + 40;
}
}
}

namespace inline_adl {
struct ParentArgument {
    int value;
};

inline namespace v1 {
int find_in_inline_namespace(ParentArgument& argument) {
    return argument.value + 50;
}

struct InlineArgument {
    int value;
};
}

int find_in_enclosing_namespace(InlineArgument& argument) {
    return argument.value + 60;
}
}

int declared_friend(FriendHost& host, int replacement) {
    host.value += replacement;
    return host.value + 2;
}

class DerivedFriendHost : public FriendHost {};

namespace friend_visibility {
class PriorVisibleHost;
int prior_visible_friend(PriorVisibleHost& host);

class PriorVisibleHost {
    int value;

    friend int prior_visible_friend(PriorVisibleHost& host);
};

int prior_visible_friend(PriorVisibleHost& host) {
    host.value = 31;
    return host.value;
}
}

template<class T>
class FriendTemplateBox {
};

class FriendTemplateHost {
    int value;

    template<class T>
    friend int reveal(T& host, int replacement) {
        host.value = replacement;
        return host.value;
    }

    template<class T>
    friend int reveal_box(T& box) {
        return 29;
    }

    template<class T>
    friend int reveal_declared(T& host, int replacement);
};

class SecondFriendTemplateHost {
    int value;

    template<class T>
    friend int reveal(T& host, int replacement);
};

class PackFriendTemplateHostA {
    int value;

    template<class T, class... Rest>
    friend int reveal_pack(T& host, int replacement, Rest... rest);
};

class PackFriendTemplateHostB {
    int value;

    template<class T, class... Rest>
    friend int reveal_pack(T& host, int replacement, Rest... rest);
};

template<class U, class... Values>
int reveal_pack(U& host, int replacement, Values... values) {
    host.value = replacement;
    return host.value;
}

template<class T, class U = T>
int reveal_default_before(T& host, int replacement);

class DefaultFriendTemplateHost {
    int value;

    template<class A, class B>
    friend int reveal_default_before(A& host, int replacement);
};

template<class X, class Y>
int reveal_default_before(X& host, int replacement) {
    host.value = replacement;
    return host.value;
}

class LateDefaultFriendTemplateHost {
    int value;

    template<class A, class B>
    friend int reveal_default_later(A& host, int replacement);
};

template<class A, class B>
int reveal_default_later(A& host, int replacement);

template<class X, class Y = X>
int reveal_default_later(X& host, int replacement) {
    host.value = replacement;
    return host.value;
}

template<class T, int N = 5, int M = N + 2>
int reveal_non_type_default_before(T& host, int replacement);

class NonTypeDefaultFriendTemplateHost {
    int value;

    template<class A, int First, int Second>
    friend int reveal_non_type_default_before(A& host, int replacement);
};

template<class U, int Count, int Extra>
int reveal_non_type_default_before(U& host, int replacement) {
    host.value = replacement + Extra;
    return host.value;
}

class LateNonTypeDefaultFriendTemplateHost {
    int value;

    template<class A, int Count>
    friend int reveal_non_type_default_later(A& host, int replacement);
};

template<class T, int N>
int reveal_non_type_default_later(T& host, int replacement);

template<class U, int Count = 11>
int reveal_non_type_default_later(U& host, int replacement) {
    host.value = replacement + Count;
    return host.value;
}

template<class T>
struct FriendTemplateDefaultBox {};

template<class T, template<class> class Container = FriendTemplateDefaultBox>
int reveal_template_default_before(T& host, int replacement);

class TemplateTemplateDefaultFriendHost {
    int value;

    template<class A, template<class> class Box>
    friend int reveal_template_default_before(A& host, int replacement);
};

template<class U, template<class> class Value>
int reveal_template_default_before(U& host, int replacement) {
    Value<U> marker;
    (void)marker;
    host.value = replacement;
    return host.value;
}

class LateTemplateTemplateDefaultFriendHost {
    int value;

    template<class A, template<class> class Box>
    friend int reveal_template_default_later(A& host, int replacement);
};

template<class T, template<class> class Container>
int reveal_template_default_later(T& host, int replacement);

template<class U, template<class> class Value = FriendTemplateDefaultBox>
int reveal_template_default_later(U& host, int replacement) {
    Value<U> marker;
    (void)marker;
    host.value = replacement;
    return host.value;
}

template<class T, int N = 1> requires (N > 0)
int reveal_constrained_default_friend(T& host, int replacement);

class ConstrainedDefaultFriendTemplateHost {
    int value;

    template<class U, int Count> requires (Count > 0)
    friend int reveal_constrained_default_friend(U& host, int replacement);
};

template<class V, int Amount> requires (Amount > 0)
int reveal_constrained_default_friend(V& host, int replacement) {
    host.value = replacement + Amount;
    return host.value;
}

template<class T>
requires requires { typename T::value_type; }
int reveal_type_requirement_friend(T& host, int replacement);

class TypeRequirementFriendTemplateHost {
public:
    using value_type = int;

private:
    int value;

    template<class U>
    requires requires { typename U::value_type; }
    friend int reveal_type_requirement_friend(U& host, int replacement);
};

template<class V>
requires requires { typename V::value_type; }
int reveal_type_requirement_friend(V& host, int replacement) {
    host.value = replacement + 1;
    return host.value;
}

template<class T>
requires requires(T candidate) {
    candidate.marker;
    typename T::value_type;
    requires sizeof(T) > 0;
    requires requires(int nested) {
        sizeof(candidate) >= sizeof(nested);
    };
    { candidate.marker } noexcept;
}
int reveal_local_requirement_friend(T& host, int replacement);

class LocalRequirementFriendTemplateHost {
public:
    using value_type = int;
    int marker;

private:
    int value;

    template<class U>
    requires requires(U probe) {
        probe.marker;
        typename U::value_type;
        requires sizeof(U) > 0;
        requires requires(int nested) {
            sizeof(probe) >= sizeof(nested);
        };
        { probe.marker } noexcept;
    }
    friend int reveal_local_requirement_friend(U& host, int replacement);
};

template<class V>
requires requires(V item) {
    item.marker;
    typename V::value_type;
    requires sizeof(V) > 0;
    requires requires(int nested) {
        sizeof(item) >= sizeof(nested);
    };
    { item.marker } noexcept;
}
int reveal_local_requirement_friend(V& host, int replacement) {
    host.value = replacement + 2;
    return host.value;
}

class DerivedFriendTemplateHost : public FriendTemplateHost {};

using FriendTemplateBox0 = FriendTemplateBox<FriendTemplateHost>;
using FriendTemplateBox1 = FriendTemplateBox<FriendTemplateBox0>;
using FriendTemplateBox2 = FriendTemplateBox<FriendTemplateBox1>;
using FriendTemplateBox3 = FriendTemplateBox<FriendTemplateBox2>;
using FriendTemplateBox4 = FriendTemplateBox<FriendTemplateBox3>;
using FriendTemplateBox5 = FriendTemplateBox<FriendTemplateBox4>;
using FriendTemplateBox6 = FriendTemplateBox<FriendTemplateBox5>;
using FriendTemplateBox7 = FriendTemplateBox<FriendTemplateBox6>;
using FriendTemplateBox8 = FriendTemplateBox<FriendTemplateBox7>;
using FriendTemplateBox9 = FriendTemplateBox<FriendTemplateBox8>;
using FriendTemplateBox10 = FriendTemplateBox<FriendTemplateBox9>;
using FriendTemplateBox11 = FriendTemplateBox<FriendTemplateBox10>;
using FriendTemplateBox12 = FriendTemplateBox<FriendTemplateBox11>;
using FriendTemplateBox13 = FriendTemplateBox<FriendTemplateBox12>;
using FriendTemplateBox14 = FriendTemplateBox<FriendTemplateBox13>;
using FriendTemplateBox15 = FriendTemplateBox<FriendTemplateBox14>;
using FriendTemplateBox16 = FriendTemplateBox<FriendTemplateBox15>;
using FriendTemplateBox17 = FriendTemplateBox<FriendTemplateBox16>;
using FriendTemplateBox18 = FriendTemplateBox<FriendTemplateBox17>;
using FriendTemplateBox19 = FriendTemplateBox<FriendTemplateBox18>;
using FriendTemplateBox20 = FriendTemplateBox<FriendTemplateBox19>;
using FriendTemplateBox21 = FriendTemplateBox<FriendTemplateBox20>;
using FriendTemplateBox22 = FriendTemplateBox<FriendTemplateBox21>;
using FriendTemplateBox23 = FriendTemplateBox<FriendTemplateBox22>;
using FriendTemplateBox24 = FriendTemplateBox<FriendTemplateBox23>;
using FriendTemplateBox25 = FriendTemplateBox<FriendTemplateBox24>;
using FriendTemplateBox26 = FriendTemplateBox<FriendTemplateBox25>;
using FriendTemplateBox27 = FriendTemplateBox<FriendTemplateBox26>;
using FriendTemplateBox28 = FriendTemplateBox<FriendTemplateBox27>;
using FriendTemplateBox29 = FriendTemplateBox<FriendTemplateBox28>;
using FriendTemplateBox30 = FriendTemplateBox<FriendTemplateBox29>;
using FriendTemplateBox31 = FriendTemplateBox<FriendTemplateBox30>;
using FriendTemplateBox32 = FriendTemplateBox<FriendTemplateBox31>;
using FriendTemplateBox33 = FriendTemplateBox<FriendTemplateBox32>;
using FriendTemplateBox34 = FriendTemplateBox<FriendTemplateBox33>;
using FriendTemplateBox35 = FriendTemplateBox<FriendTemplateBox34>;
using FriendTemplateBox36 = FriendTemplateBox<FriendTemplateBox35>;

template<class U>
int reveal_declared(U& host, int replacement) {
    host.value = replacement;
    return host.value;
}

int main() {
    FriendHost ordinary;
    DerivedFriendHost derived_ordinary;
    associated_adl::nested::Argument nested_argument = {2};
    inline_adl::ParentArgument parent_argument = {3};
    inline_adl::v1::InlineArgument inline_argument = {4};
    friend_visibility::PriorVisibleHost prior_visible;
    FriendTemplateHost host;
    SecondFriendTemplateHost second_host;
    PackFriendTemplateHostA pack_host_a;
    PackFriendTemplateHostB pack_host_b;
    DefaultFriendTemplateHost default_friend_host;
    LateDefaultFriendTemplateHost late_default_friend_host;
    NonTypeDefaultFriendTemplateHost non_type_default_friend_host;
    LateNonTypeDefaultFriendTemplateHost late_non_type_default_friend_host;
    TemplateTemplateDefaultFriendHost template_default_friend_host;
    LateTemplateTemplateDefaultFriendHost late_template_default_friend_host;
    ConstrainedDefaultFriendTemplateHost constrained_default_friend_host;
    TypeRequirementFriendTemplateHost type_requirement_friend_host;
    LocalRequirementFriendTemplateHost local_requirement_friend_host;
    DerivedFriendTemplateHost derived_host;
    FriendTemplateBox<FriendTemplateHost> box;
    FriendTemplateBox36 deep_box;
    int ordinary_result = inline_friend(ordinary, 4) +
                          declared_friend(ordinary, 5);
    return ordinary_result == 16 && reveal(host, 42) == 42 &&
           inline_friend(derived_ordinary, 6) == 7 &&
           find_in_associated_namespace(nested_argument) == 42 &&
           find_in_inline_namespace(parent_argument) == 53 &&
           find_in_enclosing_namespace(inline_argument) == 64 &&
           friend_visibility::prior_visible_friend(prior_visible) == 31 &&
           reveal_declared(host, 37) == 37 &&
           reveal(second_host, 19) == 19 &&
           reveal_pack(pack_host_a, 43, 1, 2) == 43 &&
           reveal_pack(pack_host_b, 47, 1, 2, 3) == 47 &&
           reveal_default_before(default_friend_host, 53) == 53 &&
           reveal_default_later(late_default_friend_host, 59) == 59 &&
           reveal_non_type_default_before(
               non_type_default_friend_host, 61) == 68 &&
           reveal_non_type_default_later(
               late_non_type_default_friend_host, 67) == 78 &&
           reveal_template_default_before(
               template_default_friend_host, 73) == 73 &&
           reveal_template_default_before<TemplateTemplateDefaultFriendHost>(
               template_default_friend_host, 74) == 74 &&
           reveal_template_default_later(
               late_template_default_friend_host, 79) == 79 &&
           reveal_constrained_default_friend(
               constrained_default_friend_host, 83) == 84 &&
           reveal_type_requirement_friend(
               type_requirement_friend_host, 89) == 90 &&
           reveal_local_requirement_friend(
               local_requirement_friend_host, 97) == 99 &&
           reveal(derived_host, 23) == 23 && reveal_box(box) == 29 &&
           reveal_box(deep_box) == 29 ? 0 : 1;
}
