const char* unterminated = R"tag(not closed

int main() {
    return unterminated[0];
}
