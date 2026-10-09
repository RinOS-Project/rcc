# RinCompiler namespace extension status

The C++ namespace parser reuses a namespace node when a later definition names
that namespace. Namespace-definition lookup traverses the enclosing
namespace's transitive inline-namespace set, so declarations land in the
existing ABI and lookup owner. Anonymous namespace definitions in the same
parent scope and translation unit reuse one node. Shared qualified namespace
lookup and class lookup also follow transitive inline children; namespace
lookup returns a result only when the name is unique across the inline set.

An `inline namespace` extension does not promote a namespace whose first
definition was ordinary. A namespace alias conflicts with a direct declaration
in the same namespace. A same-spelled name injected from an inline namespace
does not prevent declaring an alias in the enclosing namespace, though later
qualified lookup is ambiguous. Ambiguous namespace definitions are diagnosed;
parser recovery keeps their temporary namespace detached so invalid
declarations do not mutate an existing owner.

`test-cxx-namespace-extension` passes on both target architectures. It runs the
valid fixture, checks direct alias conflicts, ordinary-to-inline promotion,
and ambiguous namespace lookups, then compares valid/invalid behavior with GCC
C++20. The compiler was linked as `build/namespace-rcc++.exe` because the host
denied overwriting the existing `rcc++.exe`; the focused target passed using
that rebuilt executable. The broader conversion-ranking/ADL/two-phase-lookup
work and full C++20 conformance remain open in [TODO](../TODO.md).
