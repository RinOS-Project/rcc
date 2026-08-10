// C++ class test for rcc++

class Point {
public:
    int x;
    int y;

    int getX() {
        return x;
    }

    int getY() {
        return y;
    }

    int distance() {
        return x * x + y * y;
    }
};

int main() {
    int a = 10;
    int b = 20;
    int result = a + b;
    return result;
}
