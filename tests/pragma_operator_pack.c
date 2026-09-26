_Pragma("pack(push, 1)")
struct PragmaOperatorRecord {
    unsigned char tag;
    unsigned int value;
};
_Pragma("pack(pop)")

int main(void) {
    return sizeof(struct PragmaOperatorRecord) == 5u ? 0 : 1;
}
