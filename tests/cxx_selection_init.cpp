static int check_if_init(int input) {
    if (int doubled = input * 2; doubled == 6) {
        return doubled;
    }
    return -1;
}

static int check_switch_init(int input) {
    switch (int selected = input + 1; selected) {
        case 3:
            return 7;
        case 4:
            return 8;
        default:
            return -1;
    }
}

static int check_expression_init(int input) {
    int selected = 0;
    if (selected = input + 1; selected == 5) {
        return selected;
    }
    return -1;
}

int main(void) {
    return check_if_init(3) == 6 && check_if_init(1) == -1 &&
                   check_switch_init(2) == 7 &&
                   check_switch_init(3) == 8 &&
                   check_expression_init(4) == 5
               ? 0
               : 1;
}
