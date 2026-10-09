namespace hidden_friend_namespace {
class HiddenHost {
    int value;

    friend int qualified_hidden(HiddenHost& host) {
        return host.value;
    }
};
}

int main() {
    hidden_friend_namespace::HiddenHost host;
    return hidden_friend_namespace::qualified_hidden(host);
}
