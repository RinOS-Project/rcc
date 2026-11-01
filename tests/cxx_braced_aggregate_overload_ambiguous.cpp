struct NarrowAggregate {
    unsigned char value;
};

struct IntAggregate {
    int value;
};

struct Constructed {
    int value;
    Constructed(int input) : value(input) {}
};

int choose(NarrowAggregate) { return 1; }
int choose(IntAggregate) { return 2; }
int choose_constructed(Constructed);
int choose_constructed(IntAggregate);
int narrow(unsigned char) { return 3; }

int main() {
    (void)narrow({256});
    (void)choose_constructed({1});
    return choose({256});
}
