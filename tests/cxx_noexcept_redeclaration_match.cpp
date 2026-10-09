void rcc_noexcept_redecl_throwing();
void rcc_noexcept_redecl_throwing() noexcept(false);

void rcc_noexcept_redecl_nonthrowing() noexcept;
void rcc_noexcept_redecl_nonthrowing() noexcept(1);

int main() {
    return 0;
}
