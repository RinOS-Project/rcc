struct Box { int value; };

int invalid_comparison(struct Box left, struct Box right) {
    return left < right;
}

int invalid_logical(struct Box left, struct Box right) {
    return left && right;
}
