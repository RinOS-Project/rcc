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
    return 0;
}
