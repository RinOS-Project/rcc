namespace narrow {
int select(int value) {
    return value + 10;
}
}

namespace wide {
long select(long value) {
    return value + 20;
}
}

using namespace narrow;
using namespace wide;

int main(void) {
    return select(3) == 13 && select(3L) == 23 ? 0 : 1;
}
