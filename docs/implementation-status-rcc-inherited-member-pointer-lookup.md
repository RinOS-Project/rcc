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

`make SHELL=cmd.exe RCXX_TARGET=build/member-pmf-rcc++.exe
test-cxx-member-pointer-data` passed. The gate emits i686 and AMD64 output,
checks seven verified-IR functions on both targets, executes generated x64 and
GCC C++20 host binaries, and checks ambiguity and access diagnostics. The x64
host fixture exposed aggregate copy assignment through an xvalue-selected
member storing the source address instead of copying the object. Both native
backends now unwrap the reference's aggregate type and use the xvalue's object
address for the copy.

For non-template classes, an own field declared before or after the inline
method now waits for the completed class layout and lookup set. The focused
host case uses a nonzero field offset and private access, and passes on
i686/AMD64 generation and GCC C++20. Class-template cloning now keeps the
designator dependent until semantic analysis of the concrete specialization,
then resolves its final field identity, offset, and owner. The fixture uses
`int` and `double` specializations whose dependent prefix fields produce
different offsets; `test-cxx-member-pointer-data` passes both target
generations, generated x64 host execution, and GCC C++20. A unique supported
nonvirtual member-function pointer in an inline body now resolves after method
registration; its focused gate passes both targets, optimized verified IR,
generated x64 execution, and GCC C++20. A later-declared in-class static method
address in an inline body resolves as an ordinary function pointer, verified by
the same member-function gate. Full overload, virtual, owner-adjusting, and
ref/noexcept member-function-pointer forms are now implemented in the native
backends and typed SSA ([status](implementation-status-rcc-member-function-pointer-abi.md)).
Inherited data-member lookup and access contexts are complete. The separate
static-duration reference-lifetime item remains open until RinOS runtime
integration is covered ([TODO](../TODO.md)).
