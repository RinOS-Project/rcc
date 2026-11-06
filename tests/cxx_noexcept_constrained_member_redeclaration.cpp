struct ConstrainedNoexceptRedeclaration {
    template <typename T>
        requires (sizeof(T) == sizeof(double))
    ConstrainedNoexceptRedeclaration& operator=(T&&);

    template <typename T>
        requires (sizeof(T) == sizeof(double))
    ConstrainedNoexceptRedeclaration& operator=(T&&) noexcept;
};
