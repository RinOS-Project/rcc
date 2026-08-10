# rcc v3 実装状況

この一覧は「CLIが存在する」ことと「言語・ABIが完成している」ことを区別する。
未完項目が残る間はC17/C++20準拠やセルフホスト完了を宣言しない。

## 1. format / toolchain contract

- [x] `i686-unknown-rinos` / `x86_64-unknown-rinos`
- [x] `.ro v2` 64-bit section/symbol/relocation
- [x] `.ra v2` archive
- [x] canonical RIN v3 / NDRV v3 header生成
- [x] typed importとdependency metadata
- [x] external `rinsign`必須の最終link
- [x] versioned build manifestとCLI矛盾検査
- [ ] COMDAT、weak symbol、完全なarchive選択規則
- [ ] TLS、INIT/FINI、UNWIND sectionの完全link

## 2. C17 frontend

- [x] 基本declaration、function、struct/union/enum/typedef
- [x] 式、制御文、scope、基本type conversion
- [x] nested include、macro、条件付きpreprocess、`-MMD/-MF`
- [x] `_Static_assert`整数定数式と失敗diagnostic
- [ ] qualifierとeffective typeの完全なC17規則
- [ ] VLA、compound literal、designated initializerの完全実装
- [ ] `_Generic`、atomics、thread-local storage
- [ ] parser error recoveryとdiagnostic品質の網羅試験
- [ ] C17 conformance compile-and-run suite

## 3. C++20 frontend / ABI

- [x] `rcc++` entrypointとC++20既定mode
- [x] C frontendと共通のtarget/preprocessor CLI
- [ ] class、継承、virtual dispatchの完全実装
- [ ] overload resolution、namespace、ADL、two-phase lookup
- [ ] templates、concepts、constexpr/consteval、lambda
- [ ] modules、coroutines、atomics、TLS
- [ ] Itanium ABI mangling、exceptions、RTTI、static initialization
- [ ] cross-library exceptionとthread-local destructor

## 4. IR / optimization

- [ ] typed SSA IRとCFG
- [ ] target-independent MIR
- [ ] constant propagation / folding
- [ ] mem2reg、DCE、CSE/GVN
- [ ] loop optimization、inlining
- [ ] `-O0..3`ごとのpass pipeline
- [ ] linear-scan / graph-coloring register allocation

## 5. backend

- [x] i386基本integer/cdecl code generation
- [x] AMD64 SysV基本integer引数とscalar/小aggregate経路
- [x] basic global dataと`ABS32U/ABS32S/ABS64`
- [ ] i386での完全な64-bit整数演算と戻り値
- [ ] SysV aggregate分類、variadic、floating-point ABI
- [ ] PIC/PIE、GOT/PLT、TLS relocation
- [ ] DWARF debug/unwind
- [ ] inline asm constraintの完全検証
- [x] driver modeでのFPU/SIMD禁止検査

## 6. bootstrap / quality gates

- [x] preprocessor、static assert、x86_64実行ABIのhost回帰試験
- [x] SDK v1を両archの`.ra/.rll`へpackageする統合経路
- [x] production validatorによる署名付き成果物検査
- [ ] frontend/sema/IR/pass/backend単体試験の体系化
- [ ] golden `.ro/.ra/.rin/.rll/.drv`とfuzz corpus
- [ ] host stage0 -> rcc stage1 -> rcc stage2再現build
- [ ] RinOS i686セルフホスト
- [ ] RinOS x86_64セルフホスト
