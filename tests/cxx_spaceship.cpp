struct Version {
    int value;
};

int operator<=>(Version left, Version right) {
    return left.value - right.value;
}

int main() {
    Version older{2};
    Version newer{5};
    int forward = older <=> newer;
    int reverse = newer <=> older;
    return forward == -3 && reverse == 3 ? 0 : 1;
}
