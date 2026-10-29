int main(void) {
    int bound = 1;
    int first[bound++];
    int second[bound++];
    if (bound != 3 || sizeof(first) != sizeof(int) ||
        sizeof(second) != 2u * sizeof(int)) {
        return 1;
    }

    int first_typedef_bound = 3;
    int second_typedef_bound = 4;
    typedef int First[first_typedef_bound++], Second[second_typedef_bound++];
    if (first_typedef_bound != 4 || second_typedef_bound != 5) return 2;
    First first_typedef;
    Second second_typedef;
    if (sizeof(first_typedef) != 3u * sizeof(int) ||
        sizeof(second_typedef) != 4u * sizeof(int) || bound != 3) {
        return 3;
    }
    return 0;
}
