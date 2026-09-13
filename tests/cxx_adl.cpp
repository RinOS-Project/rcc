namespace geometry {

struct Point {
    int value;
};

int measure(const Point& point) {
    return point.value;
}

}

int cxx_adl_probe() {
    geometry::Point point{41};
    return measure(point);
}

int main() {
    return cxx_adl_probe() == 41 ? 0 : 1;
}
