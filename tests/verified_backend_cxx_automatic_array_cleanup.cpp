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

int verify_automatic_goto_cleanup(int* inner_order, int* outer_order,
                                  int take_goto) noexcept {
    int result = 0;
    {
        VerifiedAutomaticCleanupValue outer{
            outer_order, 39,
            {{{outer_order, 1}, {outer_order, 2},
              {outer_order, 3}},
             {{outer_order, 4}, {outer_order, 5},
              {outer_order, 6}}}};
        {
            VerifiedAutomaticCleanupValue inner{
                inner_order, 39,
                {{{inner_order, 1}, {inner_order, 2},
                  {inner_order, 3}},
                 {{inner_order, 4}, {inner_order, 5},
                  {inner_order, 6}}}};
            if (take_goto) goto after_cleanup_scope;
            result = inner.value + outer.value;
        }
    }
after_cleanup_scope:
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

int verify_automatic_switch_cleanup(int* cleanup_order,
                                    int selector) noexcept {
    int result = 0;
    switch (selector) {
        case 1: {
            VerifiedAutomaticCleanupValue value{
                cleanup_order, 39,
                {{{cleanup_order, 1}, {cleanup_order, 2},
                  {cleanup_order, 3}},
                 {{cleanup_order, 4}, {cleanup_order, 5},
                  {cleanup_order, 6}}}};
            result = value.value;
            break;
        }
        case 2: {
            VerifiedAutomaticCleanupValue value{
                cleanup_order, 39,
                {{{cleanup_order, 1}, {cleanup_order, 2},
                  {cleanup_order, 3}},
                 {{cleanup_order, 4}, {cleanup_order, 5},
                  {cleanup_order, 6}}}};
            result = value.value + 1;
            break;
        }
        default:
            return -1;
    }
    return result;
}

int verify_automatic_for_init_normal(int* cleanup_order) noexcept {
    int iterations = 0;
    for (VerifiedAutomaticCleanupValue value{
             cleanup_order, 39,
             {{{cleanup_order, 1}, {cleanup_order, 2},
               {cleanup_order, 3}},
              {{cleanup_order, 4}, {cleanup_order, 5},
               {cleanup_order, 6}}}};
         iterations < 2; ++iterations) {
        if (*cleanup_order != 0) return -1;
        if (iterations == 0) continue;
        if (*cleanup_order != 0) return -2;
    }
    return *cleanup_order == 9654321 ? iterations : -3;
}

int verify_automatic_for_init_break(int* cleanup_order) noexcept {
    int iterations = 0;
    for (VerifiedAutomaticCleanupValue value{
             cleanup_order, 39,
             {{{cleanup_order, 1}, {cleanup_order, 2},
               {cleanup_order, 3}},
              {{cleanup_order, 4}, {cleanup_order, 5},
               {cleanup_order, 6}}}};
         iterations < 3; ++iterations) {
        if (iterations == 1) break;
    }
    return *cleanup_order == 9654321 ? iterations : -1;
}

int verify_automatic_for_init_return(int* cleanup_order) noexcept {
    for (VerifiedAutomaticCleanupValue value{
             cleanup_order, 39,
             {{{cleanup_order, 1}, {cleanup_order, 2},
               {cleanup_order, 3}},
              {{cleanup_order, 4}, {cleanup_order, 5},
               {cleanup_order, 6}}}};
         true;) {
        return value.value;
    }
    return -1;
}

int verify_automatic_for_init_wide_break(int* cleanup_order) noexcept {
    long long iteration = 0;
    for (VerifiedAutomaticCleanupValue value{
             cleanup_order, 39,
             {{{cleanup_order, 1}, {cleanup_order, 2},
               {cleanup_order, 3}},
              {{cleanup_order, 4}, {cleanup_order, 5},
               {cleanup_order, 6}}}};
         iteration < 3; ++iteration) {
        if (iteration == 1) break;
    }
    return iteration == 1 ? 1 : -1;
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
    int goto_order = 0;
    int goto_outer_order = 0;
    if (verify_automatic_goto_cleanup(
            &goto_order, &goto_outer_order, 1) != 0 ||
        goto_order != 9654321 || goto_outer_order != 9654321) {
        return 3;
    }
    int fallthrough_order = 0;
    int fallthrough_outer_order = 0;
    if (verify_automatic_goto_cleanup(
            &fallthrough_order, &fallthrough_outer_order, 0) != 78 ||
        fallthrough_order != 9654321 ||
        fallthrough_outer_order != 9654321) {
        return 4;
    }
    int continue_order = 0;
    int break_order = 0;
    if (verify_automatic_loop_cleanup(&continue_order, &break_order) != 2 ||
        continue_order != 9654321 || break_order != 9654321) {
        return 5;
    }
    int switch_first_order = 0;
    if (verify_automatic_switch_cleanup(&switch_first_order, 1) != 39 ||
        switch_first_order != 9654321) {
        return 6;
    }
    int switch_second_order = 0;
    if (verify_automatic_switch_cleanup(&switch_second_order, 2) != 40 ||
        switch_second_order != 9654321) {
        return 7;
    }
    int for_normal_order = 0;
    if (verify_automatic_for_init_normal(&for_normal_order) != 2 ||
        for_normal_order != 9654321) {
        return 8;
    }
    int for_break_order = 0;
    if (verify_automatic_for_init_break(&for_break_order) != 1 ||
        for_break_order != 9654321) {
        return 9;
    }
    int for_return_order = 0;
    if (verify_automatic_for_init_return(&for_return_order) != 39 ||
        for_return_order != 9654321) {
        return 10;
    }
    int for_wide_break_order = 0;
    if (verify_automatic_for_init_wide_break(&for_wide_break_order) != 1 ||
        for_wide_break_order != 9654321) {
        return 11;
    }
    return 0;
}
