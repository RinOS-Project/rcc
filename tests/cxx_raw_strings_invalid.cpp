const char* unsupported = LR"(wide)";
const char* unterminated = R"tag(not closed

int main() {
    return unsupported[0] + unterminated[0];
}
