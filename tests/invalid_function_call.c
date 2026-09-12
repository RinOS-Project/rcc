struct Box { int value; };

int fixed_pair(int left, int right);
int explicit_void(void);
int pointer_argument(int* value);

int too_few(void) { return fixed_pair(1); }
int too_many(void) { return fixed_pair(1, 2, 3); }
int too_many_for_void(void) { return explicit_void(1); }

int incompatible_argument(void) {
    struct Box value = { 1 };
    return pointer_argument(value);
}
