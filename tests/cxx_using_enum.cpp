enum class Color : int {
    red = 1,
    blue = 2,
};

using enum Color;

namespace api {
enum class Mode {
    disabled = 0,
    enabled = 1,
};

using enum Mode;

int mode_matches(void) {
    return enabled == Mode::enabled;
}
}

enum class LocalColor { left = 4, right = 5 };

static int local_using_enum(void) {
    using enum LocalColor;
    return right == LocalColor::right;
}

int main(void) {
    Color selected = blue;
    if (selected != Color::blue || red != Color::red) return 1;
    return api::mode_matches() == 1 && local_using_enum() ? 0 : 2;
}
