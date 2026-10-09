struct RccNoexceptMemberMismatch {
    void method() noexcept;
    void method();
};
