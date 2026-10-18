struct VerifiedAutomaticCleanupLeaf {
    int* cleanup_order;
    int cleanup_id;

    ~VerifiedAutomaticCleanupLeaf() noexcept {
        *cleanup_order = *cleanup_order * 10 + cleanup_id;
    }
};

struct VerifiedAutomaticCleanupValue {
    int* cleanup_order;
    int value;
    VerifiedAutomaticCleanupLeaf children[2][3];

    ~VerifiedAutomaticCleanupValue() noexcept {
        *cleanup_order = *cleanup_order * 10 + 9;
    }
};

int verify_automatic_array_cleanup(int* cleanup_order,
                                   int early_return) noexcept {
    int result = 0;
    {
        VerifiedAutomaticCleanupValue value{
            cleanup_order, 39,
            {{{cleanup_order, 1}, {cleanup_order, 2},
              {cleanup_order, 3}},
             {{cleanup_order, 4}, {cleanup_order, 5},
              {cleanup_order, 6}}}};
        if (early_return) return value.value;
        result = value.value;
    }
    return result;
}

int verify_automatic_loop_cleanup(int* continue_order,
                                  int* break_order) noexcept {
    int iterations = 0;
    for (int index = 0; index < 3; ++index) {
        {
            int* cleanup_order = index == 0 ? continue_order : break_order;
            VerifiedAutomaticCleanupValue value{
                cleanup_order, 39,
                {{{cleanup_order, 1}, {cleanup_order, 2},
                  {cleanup_order, 3}},
                 {{cleanup_order, 4}, {cleanup_order, 5},
                  {cleanup_order, 6}}}};
            ++iterations;
            if (index == 0) continue;
            break;
        }
    }
    return iterations;
}

int main() {
    int normal_order = 0;
    if (verify_automatic_array_cleanup(&normal_order, 0) != 39 ||
        normal_order != 9654321) {
        return 1;
    }
    int early_order = 0;
    if (verify_automatic_array_cleanup(&early_order, 1) != 39 ||
        early_order != 9654321) {
        return 2;
    }
    int continue_order = 0;
    int break_order = 0;
    if (verify_automatic_loop_cleanup(&continue_order, &break_order) != 2 ||
        continue_order != 9654321 || break_order != 9654321) {
        return 3;
    }
    return 0;
}
