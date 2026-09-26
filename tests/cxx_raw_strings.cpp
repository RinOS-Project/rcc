int main() {
    const char* escaped = R"(alpha\n"beta)";
    const char* multiline = R"RIN(first
second)RIN";
    const char* utf8 = u8R"(byte)";
    const char* joined = R"(byte)" R"(string)";
    return escaped[5] == '\\' && escaped[6] == 'n' &&
                   escaped[7] == '"' && multiline[5] == '\n' &&
                   multiline[6] == 's' && utf8[3] == 'e' &&
                   joined[4] == 's'
               ? 0
               : 1;
}
