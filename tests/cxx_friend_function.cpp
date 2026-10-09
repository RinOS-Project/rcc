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
           reveal(derived_host, 23) == 23 && reveal_box(box) == 29 &&
           reveal_box(deep_box) == 29 ? 0 : 1;
}
