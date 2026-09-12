# Core object bootstrap

`bootstrap/include` is the freestanding declaration surface used while the
host-built stage0 compiler compiles its own C core. It intentionally contains
declarations only; it is not the public RinOS libc sysroot.

`make test-bootstrap-core` compiles the currently closed frontend, semantic,
optimizer, i686/AMD64 backend, preprocessor, driver-policy, object/assembly
emitters, archive/linker, RIN/RLL/NDRV v3 packagers, lexer, and C++ parser/AST
subset plus the host shim and CLI entry points twice for both RinOS
architectures and requires byte-identical `.ro v2` output.

`make test-bootstrap-link` links the rcc object closure into deterministic
unsigned RIN v3 executable images for both architectures. The images carry
explicit typed imports on `rincrt.rll`.

`make test-bootstrap-execute` maps those images through a small host-side RIN
v3 runner, applies image relocations, binds the typed imports, enforces W^X,
and executes each architecture's stage1 compiler. The stage1 output for a
probe translation unit must be byte-identical to stage0 output.

`make test-bootstrap-stage2` uses each executable stage1 compiler to rebuild
all 25 bootstrap translation units, compares every `.ro v2` object with the
stage0 result, relinks the rcc closure, and requires a byte-identical RIN v3
stage2 image. Signing, a concrete RinOS `rincrt` implementation, and execution
on RinOS remain separate gates.
