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

int main(void) {
    left::Token token{1};
    right::Flag flag{2};
    return score(token) == 11 && score(flag) == 22 ? 0 : 1;
}
