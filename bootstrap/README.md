# Core object bootstrap

`bootstrap/include` is the freestanding declaration surface used while the
host-built stage0 compiler compiles its own C core. It intentionally contains
declarations only; it is not the public RinOS libc sysroot.

`make test-bootstrap-core` compiles the currently closed frontend, semantic,
optimizer, i686/AMD64 backend, preprocessor, driver-policy, assembly-emitter,
lexer, and C++ parser/AST subset twice for both RinOS architectures and requires
byte-identical `.ro v2` output. This is a stage0-to-stage1 object gate. It does
not yet claim a linked or executable stage1 compiler; the remaining packager,
host-service shims, and stage1-to-stage2 comparison are tracked
separately.
