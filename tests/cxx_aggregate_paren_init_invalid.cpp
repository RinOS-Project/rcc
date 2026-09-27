struct Pair {
    int first;
    int second;
};

int main(void) {
    Pair pair(1, 2, 3);
    int values[2](1, 2, 3);
    return pair.first + pair.second + values[0];
}
