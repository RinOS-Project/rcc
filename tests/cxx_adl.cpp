namespace geometry {

struct Point {
    int value;
};

int measure(const Point& point) {
    return point.value;
}

int operator+(Point left, Point right) {
    return left.value + right.value;
}

}

int cxx_adl_probe() {
    geometry::Point point{41};
    return measure(point);
}

int main() {
    geometry::Point point{1};
    geometry::Point other{1};
    return cxx_adl_probe() == 41 && point + other == 2 ? 0 : 1;
}
