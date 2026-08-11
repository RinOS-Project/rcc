struct Triple {
    int first;
    int second;
    int third;
};

static int consume_triple(struct Triple value) {
    return value.first * 100 + value.second * 10 + value.third;
}

int compound_member(int value) {
    return (struct Triple){ .first = value, .second = 2, .third = 3 }.first;
}

int compound_argument(int first, int second, int third) {
    return consume_triple((struct Triple){ first, second, third });
}

int compound_array(int first, int second) {
    return ((int[3]){ first, second, 9 })[1];
}

int compound_scalar(int value) {
    return (int){ value + 2 };
}

int compound_side_effect(int* counter) {
    return consume_triple((struct Triple){ (*counter)++, 2, 3 });
}
