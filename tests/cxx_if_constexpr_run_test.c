extern int probe_if_constexpr(void);

int main(void) {
    return probe_if_constexpr() == 42 ? 0 : 1;
}
