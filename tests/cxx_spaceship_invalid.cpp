int builtin_spaceship(float left, float right) {
    return left <=> right;
}

struct CategoryLike {
    int value;
};

CategoryLike operator<=>(CategoryLike left, CategoryLike right) {
    CategoryLike result = {left.value - right.value};
    return result;
}

int invalid_spaceship_rewrite(CategoryLike left, CategoryLike right) {
    return left < right;
}
