struct IncompleteSizeofType;
int invalid_incomplete_sizeof = sizeof(struct IncompleteSizeofType);

struct IncompleteSizeofType *incomplete_sizeof_pointer;

int invalid_incomplete_sizeof_expression(void) {
    return sizeof(*incomplete_sizeof_pointer);
}
