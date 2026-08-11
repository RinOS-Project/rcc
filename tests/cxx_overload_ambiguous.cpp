long ambiguous(long value);
unsigned long ambiguous(unsigned long value);

long reject_ambiguous_overload(short value) {
    return ambiguous(value);
}
