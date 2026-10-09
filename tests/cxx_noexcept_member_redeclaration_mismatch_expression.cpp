struct RccNoexceptMemberMismatch {
    void method() noexcept(1);
    void method() noexcept(0);
};
