struct QualifiedMemberFunctionInvalidOwner {
    int read() & {
        return 1;
    }

    int read() && {
        return 2;
    }

    int throwing() {
        return 3;
    }
};

int (QualifiedMemberFunctionInvalidOwner::*throwing_as_noexcept)() noexcept =
    &QualifiedMemberFunctionInvalidOwner::throwing;

void invoke_rvalue_method_on_lvalue() {
    QualifiedMemberFunctionInvalidOwner object;
    int (QualifiedMemberFunctionInvalidOwner::*rvalue_method)() && =
        &QualifiedMemberFunctionInvalidOwner::read;
    (object.*rvalue_method)();
}
