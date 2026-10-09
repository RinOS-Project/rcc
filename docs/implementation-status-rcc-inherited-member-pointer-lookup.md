# RinCompiler inherited data-member pointer lookup status

Pointer-to-data-member formation now resolves inherited names using the
complete class lookup set. A direct declaration hides inherited declarations;
non-field declarations also hide fields, and a field/function collision from
separate bases is diagnosed as ambiguous. A `using Base::field` declaration
selects and exposes its base field, including through transitive `using` and
further-derived classes. Repeated non-virtual base subobjects are ambiguous,
while a shared virtual base identifies one field subobject.

The lookup carries both the selected declaration and the access path. It
combines field access, `using` access, and inheritance access, and preserves
the protected designating-class rule in the existing formation checks. The
positive and negative cases cover ordinary declarations, friends, member
bodies, and inline inherited lookup.

`make SHELL=cmd.exe RCXX_TARGET=build/namespace-rcc++.exe
LDFLAGS=build/debug-inline-compat.o test-cxx-member-pointer-data` passed. The
gate emits i686 and AMD64 output, checks verified IR, executes generated x64
and GCC C++20 host binaries, and checks ambiguity and access diagnostics. The
temporary compatibility object adapts an unrelated in-progress debug-inline
API migration in the shared checkout; it is not part of this change.

For non-template classes, an own field declared before or after the inline
method now waits for the completed class layout and lookup set. The focused
host case uses a nonzero field offset and private access, and passes on
i686/AMD64 generation and GCC C++20. Carrying these designators through class
template cloning remains open, as do RinOS runtime integration for the data
member-pointer parent and inline member-function-pointer resolution
([TODO](../TODO.md)).
