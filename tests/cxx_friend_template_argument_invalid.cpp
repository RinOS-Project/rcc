template<class T>
class FriendTemplateBox {};

class FriendTemplateHost {
    template<class T>
    friend int reveal_box(T& box) {
        return 29;
    }
};

class Unrelated {};

int main() {
    FriendTemplateBox<Unrelated> box;
    return reveal_box(box);
}
