extern "C" int main() {
    int left = 1;
    int right = 2;
    int both = left bitand right;
    int either = left bitor right;
    int exclusive = left xor right;
    int inverted = compl left;
    bool conjunction = (left == 1) and (right == 2);
    bool disjunction = false or true;
    bool negation = not false;
    bool different = left not_eq right;
    left and_eq 2;
    right or_eq 4;
    exclusive xor_eq 1;
    return both == 0 && either == 3 && exclusive == 2 && inverted == -2 &&
                   conjunction && disjunction && negation && different &&
                   left == 0 && right == 6 && exclusive == 2
               ? 0
               : 1;
}
