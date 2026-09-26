template<typename T>
concept Rejected = false;

int rejected(Rejected auto value) {
    return value;
}

int unknown(DoesNotExist auto value) {
    return value;
}

int main() {
    return rejected(1) + unknown(1);
}
