struct VerifiedTemporary {
    int* cleanup_order;
    int cleanup_id;
    int value;

    ~VerifiedTemporary() noexcept {
        *cleanup_order = *cleanup_order * 10 + cleanup_id;
    }
};

VerifiedTemporary make_verified_temporary(
        int* cleanup_order, int cleanup_id, int value) noexcept {
    return {cleanup_order, cleanup_id, value};
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
    if (cleanup_order != 1 || comma_order != 1) {
        return 2;
    }
    cleanup_order = 0;
    comma_order = 0;
    if (consume_verified_pair(
            (note_verified_comma(&comma_order, 1),
             make_verified_temporary(&cleanup_order, 1, 10)),
            (note_verified_comma(&comma_order, 2),
             make_verified_temporary(&cleanup_order, 2, 20))) != 30) {
        return 3;
    }
    if (comma_order == 12) return cleanup_order != 21;
    if (comma_order == 21) return cleanup_order != 12;
    return 4;
}
