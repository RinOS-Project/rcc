# RinCompiler namespace extension status

The C++ namespace parser now reuses a namespace node when a later definition
names that namespace. Namespace-definition lookup traverses the enclosing
namespace's transitive inline-namespace set, so declarations land in the
existing ABI and lookup owner. Anonymous namespace definitions in the same
parent scope and translation unit reuse one node as well.

An `inline namespace` extension does not promote a namespace whose first
definition was ordinary. Namespace aliases and ambiguous names found through
the inline namespace set are diagnosed; parser recovery keeps their temporary
namespace detached so invalid declarations do not mutate an existing owner.

`make SHELL=cmd.exe build-rcxx` passes. No regression or GCC comparison was
run for this change. Namespace extension and alias corner cases, the broader
conversion-ranking/ADL/two-phase-lookup work, and C++20 conformance remain open
in [TODO](../TODO.md).
