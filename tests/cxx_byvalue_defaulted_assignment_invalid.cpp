struct InvalidByValueDefaultedAssignment {
    InvalidByValueDefaultedAssignment& operator=(
        InvalidByValueDefaultedAssignment) = default;
};
