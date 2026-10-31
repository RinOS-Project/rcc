namespace namespace_template {

struct token {};

template<typename T>
int score(token, T value) {
    return value + 10;
}

}

namespace hidden_friend_template {

struct token {
    template<typename T>
    friend int score(token, T value) {
        return value + 20;
    }
};

}

int main() {
    namespace_template::token namespace_value;
    hidden_friend_template::token friend_value;
    return score(namespace_value, 1) == 11 &&
                   score(friend_value, 2) == 22
        ? 0 : 1;
}
