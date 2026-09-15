namespace first {
int choose(int value) {
    return value + 1;
}
}

namespace second {
int choose(int value) {
    return value + 2;
}
}

using namespace first;
using namespace second;

int main(void) {
    return choose(3);
}
