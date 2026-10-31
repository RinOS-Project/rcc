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

namespace enclosing_hidden_friend {

struct outer {
    struct token {};

    friend int score(token) {
        return 41;
    }

    template<typename T>
    friend int template_score(token, T value) {
        return value + 50;
    }
};

}

namespace enclosing_template_hidden_friend {

template<typename Owner>
struct outer {
    template<typename T>
    friend int template_score(outer, T value) {
        return value + 60;
    }
};

}

int score(hidden_friend_template::token, int value) {
    return value + 30;
}

int main() {
    namespace_template::token namespace_value;
    hidden_friend_template::token friend_value;
    enclosing_hidden_friend::outer::token nested_value;
    enclosing_template_hidden_friend::outer<int> specialized_value;
    return score(namespace_value, 1) == 11 &&
                   score(friend_value, 2) == 32 &&
                   score(friend_value, 2L) == 22 &&
                   score(nested_value) == 41 &&
                   template_score(nested_value, 3L) == 53 &&
                   template_score(specialized_value, 4L) == 64
        ? 0 : 1;
}
