# rcc v3 実装状況

この一覧は「CLIが存在する」ことと「言語・ABIが完成している」ことを区別する。
未完項目が残る間はC17/C++20準拠やセルフホスト完了を宣言しない。

## 1. format / toolchain contract

- [x] `i686-unknown-rinos` / `x86_64-unknown-rinos`
- [x] `.ro v2` 64-bit section/symbol/relocation
  - [x] host ObjectFileとrld layout/relocationの64-bit化
  - [x] 新規objectのtyped `ABS32U/ABS32S/ABS64`限定とlegacy `ABS32`拒否
- [x] `.ra v2` archive
  - [x] host Archive member sizeの64-bit化
- [x] canonical RIN v3 / NDRV v3 header生成
- [x] typed importとdependency metadata
- [x] external `rinsign`必須の最終link
- [x] versioned build manifestとCLI矛盾検査
- [ ] COMDAT、weak symbol、完全なarchive選択規則
    - [x] `.ro v2` COMDAT ANY groupの決定的選択と破損metadata拒否
    - [x] weak→strong置換時のsection/binding/size/RVA更新
    - [x] 入力順を保つ未解決symbol駆動の`.ra v2` member推移選択
- [ ] TLS、INIT/FINI、UNWIND sectionの完全link
    - [x] `.ro v2` typed sectionのRIN v3伝播、W^X/幅/metadata検証
    - [x] BSSとTLS zero-fill tailのfile size / memory size分離

## 2. C17 frontend

- [x] 基本declaration、function、struct/union/enum/typedef
  - [x] global/local文字配列のstring初期化、未指定長推論、末尾zero-fill
  - [x] global/local配列・struct・unionのbrace初期化、ネストdesignator列、zero-fill
- [x] 式、制御文、scope、基本type conversion
- [x] nested include、macro、条件付きpreprocess、`-MMD/-MF`
- [x] `_Static_assert`整数定数式と失敗diagnostic
  - [x] `_Alignof(type-name)`の定数式・static/runtime codegen
- [ ] qualifierとeffective typeの完全なC17規則
- [ ] VLA、compound literal、designator列・brace省略・上書きを含む初期化子の完全実装
  - [x] array/struct/unionの同一subobjectに対する後続initializer上書き
- [ ] `_Generic`、atomics、thread-local storage
  - [x] `_Generic`のcompatible type選択、default、非評価control
  - [x] 32-bit整数atomic load/store/exchange/CAS/fetch add/subとfull fenceの両arch codegen
  - [x] 32-bit `atomic_int/atomic_uint/atomic_flag`向け初期`<stdatomic.h>` API
  - [ ] 8/16/64-bit atomics、全標準atomic typedef/fetch bitwise、memory-order diagnostic
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
  - [x] `-O1..3`での型範囲を守るAST整数constant foldingと短絡式除去
- [ ] mem2reg、DCE、CSE/GVN
  - [x] 定数`if`分岐選択とゼロ回`while`のAST dead-code除去
- [ ] loop optimization、inlining
- [ ] `-O0..3`ごとのpass pipeline
- [ ] linear-scan / graph-coloring register allocation

## 5. backend

- [x] i386基本integer/cdecl code generation
  - [x] 宣言量に基づくstack frameとbyte/word typed load/store
- [x] AMD64 SysV基本integer引数とscalar/小aggregate経路
- [x] basic global dataと`ABS32U/ABS32S/ABS64`
  - [x] direct RIN/NDRVのDATA/CODE symbol解決と関数ポインタ
  - [x] extern/tentative definitionとzero-file-size BSS
  - [x] 文字列literalのread-only RODATA分離
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
