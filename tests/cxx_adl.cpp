namespace geometry {

struct Point {
    int value;
};

int measure(const Point& point) {
    return point.value;
}

int combine(Point point, int extra) {
    return point.value + extra;
}

long combine(Point point, long extra) {
    return point.value + extra + 100;
}

int operator+(Point left, Point right) {
    return left.value + right.value;
}

}

int cxx_adl_probe() {
    geometry::Point point{41};
    return measure(point);
}

int cxx_adl_overload_probe() {
    geometry::Point point{4};
    return combine(point, 2) == 6 ? 0 : 1;
}

int main() {
    geometry::Point point{1};
    geometry::Point other{1};
    return cxx_adl_probe() == 41 &&
                   cxx_adl_overload_probe() == 0 &&
                   point + other == 2 ? 0 : 1;
}
