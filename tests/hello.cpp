// Simple C++ test for rcc++

int debug_cpp_global = 11;
static int debug_cpp_static;

int main() {
    int x = 10;
    int y = 20;
    int sum = x + y;
    return sum + debug_cpp_global + debug_cpp_static;
}
