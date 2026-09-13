struct Point {
    int x;
    int y;

    int scaled_sum(int scale) {
        return x * scale + y;
    }

    int x_value() const {
        return x;
    }
};

int cxx_member_method_probe() {
    Point point;
    point.x = 6;
    point.y = 5;
    return point.scaled_sum(7) + point.x_value();
}

int cxx_pointer_method_probe() {
    Point point;
    point.x = 8;
    point.y = 3;
    Point* pointer = &point;
    return pointer->scaled_sum(2) + pointer->x_value();
}

int main() {
    return cxx_member_method_probe() == 53 &&
           cxx_pointer_method_probe() == 27 ? 0 : 1;
}
