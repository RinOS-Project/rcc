struct RccNoexceptMemberMismatch {
    void method();
    void method() noexcept;
};
