int select_rank(short first, long second) {
    return (int)first + (int)second;
}

long select_rank(int first, int second) {
    return (long)first + (long)second + 100;
}

int main() {
    return (int)select_rank((short)1, (short)2);
}
