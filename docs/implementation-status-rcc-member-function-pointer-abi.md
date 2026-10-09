# RinCompiler member-function pointer ABI status

Updated 2026-10-10.

RinCompiler now lowers C++ member-function pointers with the Itanium ABI
`<ptr, adj>` pair on i686 and AMD64. The pointer word contains either a direct
function address or an odd vtable byte offset for a virtual function. The
adjustment word carries the `this` displacement. Null pointers use a zero
pointer word, and owner conversion preserves null while adjusting non-null
values.

Semantic analysis resolves target-typed overloads, retains declaring owners
for inherited methods, checks access and ambiguity, and validates `const`,
`volatile`, ref, and `noexcept` function qualifiers. It accepts unique public
non-virtual owner conversions and diagnoses ambiguous or virtual-base owner
conversions. Calls apply the object/base-path adjustment and stored `this`
adjustment before direct or virtual dispatch. The native backends and typed SSA
handle the two-word object layout, equality, global initialization,
parameter/return passing, indirect calls, and null values.

`tests/cxx_member_pointer_functions.cpp` covers overload selection, inherited
methods, friend access, inaccessible and ambiguous formation, multiple
inheritance owner conversion and round-trip equality, null owner conversion,
`sizeof`, null/equality, parameter and return passing, cv/ref/noexcept
qualifiers, `noexcept` widening, virtual dispatch, and calls through virtual
base objects. Invalid fixtures check ambiguous and virtual owner conversions,
private access, invalid `noexcept` conversion, and ref-qualifier receiver
mismatch.

Verification:

- `make SHELL=cmd.exe RCXX_TARGET=build/member-pmf-rcc++.exe build-rcxx`
  passed.
- `make SHELL=cmd.exe RCXX_TARGET=build/member-pmf-rcc++.exe test-cxx-member-pointer-functions`
  passed. It emits i686 and AMD64 objects, runs generated AMD64 code on the
  host, and compares the C++20 mangled name with GCC.
- The verified-SSA fixture emits 12 functions with zero fallback notices for
  i686 and AMD64 at both `-O0` and `-O2`.
- All invalid member-function-pointer fixtures fail with their expected
  diagnostics on both targets.

Host execution is verified on AMD64. i686 is verified through object
generation and architecture inspection; RinOS target execution is not claimed.
This implementation status does not claim complete C++20 conformance or
RinOS runtime integration for every calling convention and compiler extension.
