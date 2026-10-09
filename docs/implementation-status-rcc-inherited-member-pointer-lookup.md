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

Two limits remain explicitly open in [TODO](../TODO.md): forming a pointer to
the current class's own field inside an inline member body still fails because
the class layout is incomplete during parsing, and the enclosing data-member
pointer parent requires RinOS runtime integration. Unsupported behavior is
not marked complete.
