static const char static_bytes[] = "A\0B";

int main(void) {
    char local_bytes[4] = "A\0B";
    return sizeof("A\0B") == 4 &&
           static_bytes[0] == 'A' && static_bytes[1] == '\0' &&
           static_bytes[2] == 'B' && static_bytes[3] == '\0' &&
           local_bytes[0] == 'A' && local_bytes[1] == '\0' &&
           local_bytes[2] == 'B' && local_bytes[3] == '\0' ? 0 : 1;
}
