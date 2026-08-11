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
explicit typed imports on `rincrt.rll`; signing, a concrete RinOS `rincrt`
implementation, execution on RinOS, and the stage1-to-stage2 comparison remain
separate gates.
