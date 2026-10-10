struct VerifiedChild {
    int* cleanup_order;
    int cleanup_id;

    ~VerifiedChild() noexcept {
        *cleanup_order = *cleanup_order * 10 + cleanup_id;
    }
};

struct VerifiedTemporary {
    int* cleanup_order;
    int cleanup_id;
    int value;
    VerifiedChild child;

    ~VerifiedTemporary() noexcept {
        *cleanup_order = *cleanup_order * 10 + cleanup_id;
    }
};

struct VerifiedPairTemporary {
    int* cleanup_order;
    int cleanup_id;
    int value;

    ~VerifiedPairTemporary() noexcept {
        *cleanup_order = *cleanup_order * 10 + cleanup_id;
    }
};

struct VerifiedWordTemporary {
    int* cleanup_count;

    ~VerifiedWordTemporary() noexcept {
        *cleanup_count += 1000;
    }
};

struct VerifiedValueRoot {
    int value;
};

struct VerifiedValuePrefix {
    int prefix_value;
};

struct VerifiedValueLeaf : VerifiedValuePrefix, VerifiedValueRoot {
    int* cleanup_count;

    ~VerifiedValueLeaf() noexcept {
        ++*cleanup_count;
    }
};

VerifiedTemporary make_verified_temporary(
        int* cleanup_order, int cleanup_id, int value) noexcept {
    return {cleanup_order, cleanup_id, value,
            {cleanup_order, cleanup_id + 1}};
}

int consume_verified_temporary(const VerifiedTemporary& value) noexcept {
    return value.value;
}

int consume_verified_pair(const VerifiedTemporary& first,
                          const VerifiedTemporary& second) noexcept {
    return first.value + second.value;
}

VerifiedPairTemporary make_verified_pair_temporary(
        int* cleanup_order, int cleanup_id, int value) noexcept {
    return {cleanup_order, cleanup_id, value};
}

int consume_verified_pair_temporary(
        const VerifiedPairTemporary& value) noexcept {
    return value.value;
}

VerifiedWordTemporary make_verified_word_temporary(
        int* cleanup_count) noexcept {
    return {cleanup_count};
}

int consume_verified_word_temporary(
        const VerifiedWordTemporary& value) noexcept {
    return *value.cleanup_count;
}

VerifiedWordTemporary make_word_from_temporary(
        int* cleanup_count, const VerifiedTemporary& source) noexcept {
    *cleanup_count += source.value;
    return {cleanup_count};
}

VerifiedValueLeaf make_verified_value_leaf(int* cleanup_count) noexcept {
    return {{17}, {73}, cleanup_count};
}

int consume_verified_value_root(VerifiedValueRoot value, int* cleanup_count,
                                int* observed_cleanup_count) noexcept {
    *observed_cleanup_count = *cleanup_count;
    return value.value;
}

int note_verified_comma(int* comma_order, int comma_id) noexcept {
    *comma_order = *comma_order * 10 + comma_id;
    return 0;
}

int main() {
    int cleanup_order = 0;
    int comma_order = 0;
    if (consume_verified_temporary(
            (note_verified_comma(&comma_order, 1),
             make_verified_temporary(&cleanup_order, 1, 42))) != 42) {
        return 1;
    }
    if (cleanup_order != 12 || comma_order != 1) {
        return 2;
    }
    cleanup_order = 0;
    comma_order = 0;
    if (consume_verified_pair(
            (note_verified_comma(&comma_order, 1),
             make_verified_temporary(&cleanup_order, 1, 10)),
            (note_verified_comma(&comma_order, 2),
             make_verified_temporary(&cleanup_order, 3, 20))) != 30) {
        return 3;
    }
    if (comma_order == 12) {
        if (cleanup_order != 3412) return 4;
    } else if (comma_order == 21) {
        if (cleanup_order != 1234) return 4;
    } else {
        return 4;
    }

    comma_order = 0;
    cleanup_order = 0;
    if (consume_verified_temporary(
            true ? make_verified_temporary(&cleanup_order, 5, 52)
                 : make_verified_temporary(&cleanup_order, 7, 74)) != 52) {
        return 5;
    }
    if (cleanup_order != 56) return 6;

    cleanup_order = 0;
    if (consume_verified_temporary(
            false ? make_verified_temporary(&cleanup_order, 5, 52)
                  : make_verified_temporary(&cleanup_order, 7, 74)) != 74) {
        return 7;
    }
    if (cleanup_order != 78) return 8;

    cleanup_order = 0;
    if (consume_verified_pair_temporary(
            true ? make_verified_pair_temporary(&cleanup_order, 9, 92)
                 : make_verified_pair_temporary(&cleanup_order, 7, 74)) != 92) {
        return 9;
    }
    if (cleanup_order != 9) return 10;

    cleanup_order = 0;
    if (consume_verified_pair_temporary(
            false ? make_verified_pair_temporary(&cleanup_order, 9, 92)
                  : make_verified_pair_temporary(&cleanup_order, 7, 74)) != 74) {
        return 11;
    }
    if (cleanup_order != 7) return 12;

    int left_cleanup_count = 31;
    int right_cleanup_count = 57;
    if (consume_verified_word_temporary(
            true ? make_verified_word_temporary(&left_cleanup_count)
                 : make_verified_word_temporary(&right_cleanup_count)) != 31) {
        return 13;
    }
    if (left_cleanup_count != 1031 || right_cleanup_count != 57) return 14;

    left_cleanup_count = 31;
    right_cleanup_count = 57;
    if (consume_verified_word_temporary(
            false ? make_verified_word_temporary(&left_cleanup_count)
                  : make_verified_word_temporary(&right_cleanup_count)) != 57) {
        return 15;
    }
    if (left_cleanup_count != 31 || right_cleanup_count != 1057) return 16;

    int nested_cleanup_order = 0;
    int nested_result = 20;
    if (consume_verified_word_temporary(
            true ? make_word_from_temporary(
                       &nested_result,
                       make_verified_temporary(&nested_cleanup_order, 5, 32))
                 : make_word_from_temporary(
                       &nested_result,
                       make_verified_temporary(&nested_cleanup_order, 7, 43)))
            != 52) {
        return 17;
    }
    if (nested_cleanup_order != 56) return 18;
    if (nested_result != 1052) return 21;

    nested_cleanup_order = 0;
    nested_result = 20;
    if (consume_verified_word_temporary(
            false ? make_word_from_temporary(
                        &nested_result,
                        make_verified_temporary(&nested_cleanup_order, 5, 32))
                  : make_word_from_temporary(
                        &nested_result,
                        make_verified_temporary(&nested_cleanup_order, 7, 43)))
            != 63) {
        return 19;
    }
    if (nested_cleanup_order != 78) return 20;
    if (nested_result != 1063) return 22;

    int derived_value_cleanup_count = 0;
    int derived_value_cleanup_count_during_call = -1;
    if (consume_verified_value_root(
            make_verified_value_leaf(&derived_value_cleanup_count),
            &derived_value_cleanup_count,
            &derived_value_cleanup_count_during_call) != 73) {
        return 23;
    }
    if (derived_value_cleanup_count !=
        derived_value_cleanup_count_during_call + 1) return 24;
    return 0;
}
