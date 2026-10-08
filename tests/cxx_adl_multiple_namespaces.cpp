namespace left {
struct Token {
    int value;
};

int score(Token token) {
    return token.value + 10;
}
}

namespace right {
struct Flag {
    int value;
};

int score(Flag flag) {
    return flag.value + 20;
}
}

namespace base_payload {
struct Base {};
}

namespace wrapper {
template<typename T>
struct Box {};

struct Derived : base_payload::Base {};
}

namespace payload {
struct Token {
    int value;
};

int score_box(wrapper::Box<Token>) {
    return 33;
}
}

namespace base_payload {
int score_base(wrapper::Derived) {
    return 44;
}
}

namespace member_payload {
struct Owner {
    int value;
};

int score_member(int Owner::*) {
    return 55;
}
}

int main(void) {
    left::Token token{1};
    right::Flag flag{2};
    wrapper::Box<payload::Token> box;
    wrapper::Derived derived;
    int member_payload::Owner::* member = &member_payload::Owner::value;
    return score(token) == 11 && score(flag) == 22 &&
                   score_box(box) == 33 && score_base(derived) == 44 &&
                   score_member(member) == 55
        ? 0 : 1;
}
