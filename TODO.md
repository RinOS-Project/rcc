# rcc v3 実装状況

## RCC continuation (2026-10-30, floating math builtins)

- [x] Validate argument counts and types for floating classification,
  `__builtin_fpclassify`, `copysign`, and the `floor`/`ceil`/`round`/`trunc`/
  `rint` families. Implement i686 and AMD64 legacy lowering for binary32,
  binary64, and RinOS binary64 `long double`. Runtime-test C and C++ generated
  code on both hosts, check all four dynamic rounding modes for AMD64 `rint`/
  `rintl`, preserve negative zero, and diagnose invalid argument forms through
  `test-floating-math-builtins`.
- [x] Lower classification, sign-copy, and fixed-direction rounding builtins
  into typed SSA for AMD64 binary32/binary64 and i686 binary32; the focused
  verified-backend checks report zero legacy fallback. i686 floating
  classification's 64-bit raw representation now uses the actual EDX:EAX
  result pair in legacy code generation.
- [x] Lower i686 binary64 classification, `fpclassify`, `signbit`,
  `copysign`, and fixed rounding builtins through explicit low/high i32 values
  in typed IR. C17/C++20 verified-backend checks report zero fallback; the
  existing i686 legacy C/C++ runtime checks still pass.
- [x] Add typed-IR `frint` semantics that observe the active rounding
  environment, with x87 `FRNDINT` lowering on i686 and x86-64. C17/C++20
  verified builds report zero fallback on both targets. Execute verified
  objects at O0 and O2 on i686 and x86-64 under all four rounding modes for
  `rintf`, `rint`, and RinOS binary64 `rintl`; verify negative zero and
  `FE_INEXACT` when the result is discarded. Legacy runtime coverage remains
  on both targets.
- [x] Define the GCC-compatible `__DBL_MIN__` preprocessing value required by
  RinOS math headers. Check C/C++ builtin-macro fixtures and run
  `test-preprocessor-has-builtin` plus `test-floating-math-builtins`.

## RCC continuation (2026-10-29, command-line include search order)

- [x] Search command-line `-I` paths before executable-relative and bootstrap
  headers in both C and C++ drivers. A `limits.h` override fixture compiles in
  C17/C++20 for i686 and AMD64; `test-tool-relative-includes` passes, including
  compiler invocation from a different working directory and an installed
  compiler layout, and is part of `test-c17`/`test-ci`.

## RCC continuation (2026-10-29, C23 noreturn attribute)

- [x] Parse C23 `[[noreturn]]` in C declaration attribute sequences, retain
  its function-declaration effect across declaration specifiers and
  declarators, accept multiple declarators, and reject arguments or
  non-function application. Preserve the existing C++ attribute parser and
  gate the C spelling to C23 mode.
- [x] Implement C23 `__has_c_attribute` preprocessing, including macro-expanded
  names, supported alias spellings, namespace-prefixed/unknown names, and
  malformed-argument diagnostics. Add the dual-target regression to
  `test-c23` and `test-ci`; C23 tests and focused C17/C++ compatibility tests
  pass.
- [x] Implement C23 `[[nodiscard]]` on function declarations and struct/union/
  enum definitions, including a string-literal message, direct discarded-call
  diagnostics, type-return diagnostics, `(void)` suppression, and the C23
  first-attributed-declaration redeclaration rule. Reject malformed arguments,
  variables, incomplete type declarations, and C17 use. Verify dual-target
  code generation, x64 execution, GCC C2x syntax/runtime behavior,
  `__has_c_attribute`, GCC C2x syntax and execution, and existing C++20
  nodiscard/constexpr regressions in `test-c23-nodiscard-attribute`,
  `test-cxx-nodiscard`, and focused gates.
- [x] Implement and test the remaining standard C attributes and their
  semantics (`fallthrough`, `unsequenced`, and `reproducible`); accepting or
  ignoring unknown attributes is not equivalent to implementing them.
  - [x] Implement C23 `[[deprecated]]` for functions, objects, struct/union
    members, struct/union/enum tags, typedef names, and enumerators. Preserve
    messages/redeclarations and emit one diagnostic per use; advertise
    `__has_c_attribute(deprecated) == 202311L`. Verify C17 misuse and malformed
    arguments, i686/AMD64 generation, and GCC C2x overlap in
    `test-c23-deprecated-attribute`, which is part of `test-c23`/`test-ci`.
  - [x] Diagnose C23 `[[deprecated]]` on a parameter as outside the standard
    attribute's appertainment constraint while retaining it as an explicit
    extension; the extension warns when the parameter is referenced.
  - [x] Implement C23 `[[maybe_unused]]` on object/function/typedef/tag/member/
    enumerator/label declarations and parameters. Retain its entity marker
    through variable/function/typedef redeclarations, reject arguments and
    pre-C23 use, and advertise `__has_c_attribute(maybe_unused) == 202311L`.
    Verify the standard contexts on both targets, strict GCC C2x syntax, and
    invalid-context diagnostics in `test-c23-maybe-unused-attribute`.
    RCC currently has no general unused-entity warnings; the attribute's
    warning suppression follows the standard's recommended practice.
  - [x] Implement C23 `[[fallthrough]]` as a standalone attribute declaration.
    Enforce the innermost-switch next-label and iteration-body constraints;
    advertise `__has_c_attribute(fallthrough) == 202311L`; reject arguments,
    non-declaration placement, and pre-C23 use. Gate both-target syntax and
    constraint regressions and x64 host execution in
    `test-c23-fallthrough-attribute`; the full `test-c23` and focused C++20
    deprecated/nodiscard gates pass.
  - [x] Implement C23 `[[reproducible]]` and `[[unsequenced]]` on function
    declarators and function type specifiers. Preserve each property on the
    function type, keep attributed/unattributed function types compatible, and
    propagate properties from visible redeclarations to function definitions
    and function-pointer objects. Reject arguments, non-function types, and
    pre-C23 use; report `__has_c_attribute` value `202311L`. Verify function
    typedefs, nested/array pointers, parameters, members, type-name parsing,
    comma declarators, redeclarations, both target outputs, and x64 execution
    in `test-c23-function-type-attributes`.
  - Progress: `deprecated` no longer has unsupported valid-position errors.
    GCC's current C2x frontend does not provide a tag-attribute diagnostic
    oracle, so RCC's struct/enum tag positions are directly covered by the
    dual-target regression. All standard C attributes are implemented and
    covered by the `test-c23` gate; focused C++20 deprecated/nodiscard
    regressions also pass.

## RCC continuation (2026-10-29, C23 bit-precise integers)

- [x] Publish `BITINT_MAXWIDTH` as 64 and exercise signed/unsigned `_BitInt`
  types at 1/2/5/9/33/64-bit widths, including minimum/maximum values,
  conversion truncation, narrow unsigned wraparound, signed arithmetic,
  `wb`/`uwb`/`WB`/`UWB` literals, and 64-bit literal boundaries. Clang 18
  C2x compiled and executed the same semantic fixture after omitting only RCC's
  implementation-specific `BITINT_MAXWIDTH == 64` assertion.
- [x] Add `test-c23-bitint` to `test-c23` and `test-ci`. Verify i686/AMD64
  objects, x86_64 host execution, direct RIN plus RLD-linked RIN, and direct
  RLL/DRV output using `rinvalidate`; verify width diagnostics and C17/C++20
  mode rejection.
- [x] Verify signed and unsigned `_BitInt` bit-fields, including 5-bit fields
  based on 9-bit types and a 33-bit field; execute sign/value preservation and
  boundary reads on the x86_64 host and validate both targets' RIN/RLL/DRV
  outputs through the `test-c23-bitint` gate.
- [x] Verify `_Generic` exact-type selection for bit-precise types and usual
  arithmetic conversions across narrower/equal/wider mixed ranks; exercise
  defined unsigned narrow-width left shifts including modulo wraparound.
  Verify unary `+`, `-`, and `~` preserve narrow bit-precise types without
  integer promotion and produce width-truncated signed/unsigned results.
- [x] Verify variadic retrieval preserves bit-precise types: signed/unsigned
  `_BitInt(5)`, unsigned `_BitInt(33)`, and `_BitInt(64)` are read with their
  original types; execute on the x86_64 host and validate both targets' RIN/
  RLL/DRV outputs through `test-c23-bitint`.
- [x] Verify macro-expanded signed/unsigned 64-bit `wb` constants in C23
  `#if` evaluation against the corresponding standard `LL`/`ULL` constants.
- [x] Evaluate typed integer shifts consistently in static-storage initializers
  and C++20 constexpr expressions. Preserve unsigned 64-bit top-bit shifts,
  accept representable signed shifts, and reject negative signed left operands,
  signed overflow, and out-of-range counts; regress C23 `_BitInt(64)` static
  assertions/initializers/case labels and C++ constexpr switch/compound shifts.
  `test-c23-bitint` and `test-cxx-constexpr` pass for i686/AMD64 outputs and
  AMD64 host execution.
- [x] Verify C23 integer-to-floating and floating-to-bit-precise-integer
  conversions, including signed 9/33-bit values, unsigned 33/64-bit values,
  truncation toward zero, mixed integer/floating arithmetic, and float/double
  inputs immediately below and at 2^63 plus values near 2^64. Fix x86-64
  legacy code generation for unsigned 64-bit integer-to-floating conversion
  and floating-to-unsigned conversion by splitting the upper half before SSE
  conversion. Fix i686 x87 floating-to-unsigned conversion by correcting its
  half-range branch and restoring bit 63 in the high result word.
  `test-c23-bitint-float-conversion` is part of `test-c23`/`test-ci`; it
  compiles and assembles i686 output, compiles AMD64 legacy and verified
  objects with zero fallback, and runs AMD64 output on the host. The generated
  i686 code was also linked and executed under WSL and returned zero. The
  installed GCC does not accept `_BitInt`; Clang 18 compiled and executed the
  conversion fixture successfully as an independent oracle.
- [ ] Audit remaining C23 bit-precise integer rules and contexts, especially
  promotions, conversions, invalid expressions, and overflow contexts.
  - [x] Preserve the declared `_BitInt` type for narrow bit-precise bit-fields
    through unary `+`, unary `-`, `~`, and shifts, following C23 6.3.1.1. The
    N3220 wording differs from Clang 18's AST promotion result for this case,
    so Clang 18 is not used as the conformance oracle. Verify exact types,
    AMD64 runtime behavior, i686/AMD64 RIN/RLL/DRV generation and validation,
    plus rejection of signed `_BitInt` addition/division overflow, invalid
    shift counts, negative signed shift operands, and signed shift overflow in
    ICEs.
  - [x] Verify conditional common types, unsigned wraparound through
    postincrement and compound assignment, switch control types, and duplicate
    `case` values after conversion to a narrow unsigned `_BitInt` type.
  - [x] Cover usual arithmetic conversion rank/range boundaries against
    `long long`: unsigned `_BitInt(63)` converts to signed `long long` because
    its full range is representable, while signedness and equal-width
    `_BitInt(64)` cases select `unsigned long long` or `long long` as required.
    Verify result types at compile time and values at runtime through the
    i686/AMD64 RIN/RLL/DRV and `rinvalidate` regression gate.

## RCC continuation (2026-10-29, C23 binary resource inclusion)

- [x] Implement C23-only `#embed` and `__has_embed` using the compiler's
  include-path search policy. Read resource bytes with explicit lengths,
  preserve embedded NULs, honor `limit` without loading bytes past the selected
  count, and track embedded resources in `-MMD` dependency output.
- [x] Support `limit`, `prefix`, `suffix`, and `if_empty`, including the
  standard double-underscore spellings; report NOT_FOUND/EMPTY/FOUND from
  `__has_embed`, and diagnose malformed, duplicate, negative, and unsupported
  parameters. Keep C17 `#embed` on the existing unknown-directive diagnostic
  path.
- [x] Add `test-c23-embed` to `test-c23` and therefore `test-ci`. Native
  Windows verifies quoted/angle and macro-expanded names, empty and NUL-byte
  resources, dependency output, i686/AMD64 objects, host execution, invalid
  diagnostics, and the C17 mode boundary.

## RCC continuation (2026-10-28, C23 typeof type-name operands)

- [x] Add C23-only `typeof` / `typeof_unqual` tokens and type-name parsing;
  preserve the spellings as identifiers in C17 and C++.
- [x] Preserve qualifiers in `typeof`; remove top-level and array-element
  qualifiers plus atomic qualification in `typeof_unqual`, while retaining
  pointee qualifiers. Add nested type-name, function-pointer, C17 identifier,
  i686/AMD64 object, AMD64 execution, and const-write diagnostic coverage.
- [x] Resolve ordinary expression operands without runtime evaluation; test
  identifiers, arithmetic, pointer/function/array/string/call expressions,
  and side effects that must remain unevaluated for non-VM operands.
- [x] Restrict deferred typedef-type resolution to `DECL_TYPEDEF` so the
  overlapping function-declaration union member is never misread as a type;
  the full C17 suite, including atomic builtin regressions, passes.
- [x] Reject dot and arrow expression operands that designate bit-field
  members, and test array/function parameter adjustment through
  `typeof(parameter)`. Resolve type operands in typedefs and `_Generic`
  associations, with runtime coverage for both contexts.
- [x] Evaluate VM expression operands in declarations and `sizeof` type-name
  contexts; preserve original type identity and saved VLA extents across
  `typeof` copies. Cover nested `typeof`, pointer and array VM operands, and
  side effects in both expression and type-name VLA operands.
- [x] Capture each block-scope VLA typedef bound when its declaration is
  reached, and reuse that saved bound for declarations using the typedef.
  Verify side-effect timing, subsequent changes to the bound variable, and
  repeated execution of the typedef inside a loop on native i686/AMD64;
  compare the same runtime fixture with GCC 13.3.
- [x] Capture independent bounds for multiple typedef declarators in one
  declaration and reuse them through nested array typedef aliases; verify
  i686/AMD64 execution and GCC 13.3 / Clang 18 runtime oracles.
- [x] Defer C23 `__builtin_types_compatible_p` and `__builtin_offsetof`
  operands containing `typeof` until semantic type resolution; cover aggregate,
  pointer and member-array compatibility plus nested member/index offsets with
  i686/AMD64 objects, AMD64 execution, and a GCC `-std=c2x -Werror` oracle.
- [x] Resolve the `typeof_unqual(const int[3])` oracle discrepancy: C23 N3220
  6.7.3.6p5 and 6.7.4p10 require removing qualification from the array and its
  element; GCC PR112841 confirms the GCC 13 behavior was fixed in GCC 15.
  Retain the mutable-array runtime regression; older GCC/Clang results are not
  the conformance oracle for this case.
- [x] Cover function prototype scope for ordinary VLA pointers and `typeof`
  array operands: different non-constant bounds are treated as `[*]` for
  compatibility/parameter adjustment. Verify i686/AMD64 objects and GCC C2x
  syntax with only GCC's `-Wvla-parameter` compatibility warning disabled.
- [x] Verify left-to-right evaluation across multiple block-scope declarators
  using a shared counter for ordinary VLAs and multiple VLA typedef names;
  check the captured extents on RCC x86_64 and GCC C2x runtime, plus RCC i686
  object generation.
- [x] Reuse a VLA bound from one `typeof(type-name)` specifier across comma-
  separated declarators, while evaluating a separately written specifier again.
  Verify extents and side-effect counts with GCC 13.2 C2x, RCC x86_64 runtime,
  and RCC i686/x86_64 object generation.
- [x] Capture a `typeof(type-name)` VLA bound at a block-scope typedef
  declaration, then reuse its saved extent across later objects without
  reevaluating the bound. Verify declaration timing and `sizeof` results with
  GCC C2x and RCC x86_64 runtime plus i686/x86_64 object generation.
- [x] Compose `typeof` VLA typedefs (`Row`, then `Matrix`) and reuse the inner
  typedef's captured extent while evaluating only the new outer bound; verify
  both typedef points and later object sizes with the same runtime/object
  matrix.
- [x] Accept compatible prototype-scope redeclarations with nested
  `typeof(type-name)` VLA bounds of different non-constant sizes; verify GCC
  C2x syntax and RCC i686/x86_64 object generation.
- [x] Audit prototype-scope typeof parameters using block-scope VLA typedefs,
  direct and nested typeof VM operands; add a regression proving prototype
  declarations do not evaluate their size operands. Check a typeof VM bound
  combined with a declarator VLA bound on independent objects, without
  assuming a relative evaluation order.
- [ ] Audit the remaining sequencing cases. WG14 issue 1120 is open on
  full-expression boundaries for typeof/declarator size expressions, so do
  not treat one compiler's ordering as a general conformance oracle.
  - [x] Evaluate VLA size expressions in cast type-names and semantically
    resolve their bounds before code generation. Cover a VM `typeof` expression
    whose type comes from a pointer-to-VLA cast, an independent bound in the
    enclosing declarator, and `typeof(type-name)` reused by comma declarators.
    Check each independent side effect without asserting relative order;
    generate i686/AMD64 objects and run the AMD64 RCC/GCC C2x fixtures. GCC
    reevaluates a VM `typeof` expression for multiple declarators, so that
    issue-1120 question is not used as a once-only oracle. Temporary-lifetime
    cases whose full-expression boundary remains disputed stay open.
  - [x] Make the same-declaration VLA typedef regression use independent
    counters, checking each bound is captured once without asserting the
    relative evaluation order of distinct size expressions.
  Native i686 execution now passes for the covered typedef/runtime cases.

この一覧は「CLIが存在する」ことと「言語・ABIが完成している」ことを区別する。
未完項目が残る間はC17/C++20準拠やセルフホスト完了を宣言しない。

## RCC continuation (2026-10-27, inherited-constructor target build)

- [x] Publish the inherited-constructor member-initializer completion API in
  `include/ast.h` and remove its GNU/Clang-only weak declaration from
  `src/sema.c`. RinOS-target builds now compile the caller with the same real
  parser implementation linked into both compiler images.
- [x] Pass `make SHELL=cmd.exe test-bootstrap-stage2` (exit 0): reproducible
  i686/x86_64 stage0 core objects and stage1 RINs, stage1 execution through
  the host runner, and reproducible stage2 images for both targets.
- [ ] Run the resulting compiler natively on RinOS for i686 and x86_64;
  host-runner bootstrap does not establish target-OS execution.

## 実装状況の再監査 (2026-10-08)

以下はsource、regression target、check-in済みCI workflowに基づく分類であり、
標準準拠率の推定ではない。限定実装のあるsubsystemは、残る意味論・ABI coverageまで
確認しない限り完了扱いにしない。

- C17はfrontend、ABI、optimizer、host executionの広い範囲が
  `C17_REGRESSION_TARGETS`にあるが、ISO C17完全準拠を意味しない。
- C++にはclass、template/concept/lambda、exception、bounded RTTI/`dynamic_cast`、
  static initializationの実装と専用テストがある。C++20全準拠は未達で、
  modules/coroutinesおよびABI/template corner caseが残る。
- `.ro v2`、`.ra v2`、RIN v3、NDRV v3にはproduction emit/link/validation経路がある。
  PIC/GOT/PLTとlocal-exec TLSも限定modelで実装済みだが、visibility、interposition、
  TLS model、shared-library互換性は未完了。
- typed SSA、MIR、register allocation、optimization、verified object backendは
  実経路として存在する。`-fverified-backend`は明示opt-inで、未対応functionは
  legacy backendへfallbackする。aggregate/vector/exceptionやtranslation unit全体の
  coverageは未完了。
- bounded scalar inlining、LICM、loop unrolling、strength reductionは実装済み。
  一般のloop transformationとcost-aware/interprocedural optimizationは未完了であり、
  「inliningやloop optimizationが存在しない」とは分類しない。
- DWARF line/info/frame、stack location、多数のC/C++ type DIEと、nested
  lexical block内stack localのv4 location listは実装済みだが、全lifetime／
  register配置を追跡する完全なlocation list、全inline attribution、任意prologueの
  CFIは未完了。verified backendでは最適化inlineのorigin／call-site rangeを
  i686/AMD64でDWARFへ出力する経路を追加済み。
  RinOS-native i686/x86_64 self-hostは、成功済みhost stage2 reproductionとは別項目。
- `.github/workflows/ci.yml`はGCC/Clangの`test-ci`、full `test-cxx`、独立sanitizer jobを
  設定済み。workflowの存在だけでは成功を証明しないため、下記CI実行確認項目は
  成功runを記録するまで未完了のままにする。

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
- [x] `.ro v2`契約内のCOMDAT、weak symbol、archive選択規則
    - [x] `.ro v2` COMDAT ANY groupの決定的選択と破損metadata拒否
    - [x] weak→strong置換時のsection/binding/size/RVA更新
    - [x] 入力順を保つ未解決symbol駆動の`.ra v2` member推移選択
    - [x] C/C++ source-level `__attribute__((weak))` and C++20
      `[[gnu::weak]]` declarations, weak definitions, and weak imports
      through AST/sema, native/verified backends, explicit argument/member
      diagnostics, and i686/AMD64 `.ro` regression coverage
    - [ ] 他形式・全ABI edge caseを含む完全なCOMDAT/weak/archive互換性
- [x] `.ro v2`からRIN v3へ伝播するTLS、INIT/FINI、UNWIND section metadata
    - [x] `.ro v2` typed sectionのRIN v3伝播、W^X/幅/metadata検証
    - [x] BSSとTLS zero-fill tailのfile size / memory size分離
    - [ ] 実行時TLS destructorと完全なUNWIND personality/runtime連携

## 2. C17 frontend

- [x] 基本declaration、function、struct/union/enum/typedef
  - [x] C17/C++20列挙子値のconditional-expression定数評価、C17列挙子のint範囲検証、
        先行列挙子参照と非定数／範囲外diagnosticを追加
  - [x] global/local文字配列のstring初期化、未指定長推論、末尾zero-fill
  - [x] 文字列リテラルの埋め込みNULを長さ付きbyte列として保持し、隣接連結、
        static/TLS/local初期化、IR、C++文字列UDLのlength引数へ伝播
  - [x] global/local配列・struct・unionのbrace初期化、ネストdesignator列、zero-fill
- [x] 式、制御文、scope、基本type conversion
  - [x] C17 integer literalの基数別候補型、`U/L/LL` suffix、overflow診断
  - [x] ILP32/LP64の通常算術変換と型付きunsigned整数定数式
  - [x] shift・単項演算のinteger promotionと左辺基準result type
  - [x] cast・代入・local初期化・return・固定引数callの整数変換codegen
  - [x] prototype有無、call arity・固定引数互換性、default integer promotion
  - [x] 通常算術変換に従うsigned/unsigned比較と64-bit scalar truth判定
  - [x] 全integer compound assignmentと左辺一回評価の両arch codegen
  - [x] `switch/case/default`のfallthrough、nested context、64-bit dispatchとdiagnostic
  - [x] 裸の`signed` / `unsigned`を`int`として解釈
  - [x] 両archのfunction-local `goto` / label loweringと配置diagnostic
- [x] C17の`return`制約として、void関数のreturn-expressionと非void関数の
      値なしreturnをsemaで拒否し、i686/AMD64診断と有効なvoid return実行を検証
- [x] `f` suffix付き浮動小数点リテラルの型保持と、定数式による
      float/doubleのstatic/TLS IEEE scalar初期化
- [x] x86-64 runtimeのfloat/double算術・比較・cast・代入と、
      register-only SysV XMM scalar引数/戻り値lowering
  - [x] typed-SSA scalar castでsigned/unsigned integerとf32/f64の変換、
        f64→f32 narrowingを実装。uint64→floatの丸めと、範囲内float→uint64を
        境界値および決定的512ケースのhost differentialで照合し、C/C++の
        x64通常/O2生成・実行でfallbackなしを検証
  - [x] i686向け整数/floating conversion追加後もx86-64 unsigned 32-bit→floatが
        zero-extensionを保つことを`UINT32_MAX`のtyped-SSA object executionで検証し、
        `test-verified-backend`全体を再実行
- [x] i686 runtimeのfloat/double x87算術・比較・cast・代入・前後置更新と、
      cdecl stack引数/スカラー戻り値lowering
- [x] nested include、macro、条件付きpreprocess、`-MMD/-MF`
  - [x] C17/C++20 translation phase 1としてCRLF/CRをLFへ正規化してから
        line-splice、comment除去、literal lexingを行い、raw stringの埋め込み
        改行をcheckoutの改行形式に依存させない
- [x] `_Static_assert`整数定数式と失敗diagnostic
  - [x] `_Alignof(type-name)`の定数式・static/runtime codegen
- [ ] qualifierとeffective typeの完全なC17規則
  - [x] 前置・後置cv指定、pointer level cv、modifiable lvalue検査
  - [x] `volatile` object/pointerの未使用readに対するDCE抑止
  - [x] C17/C++20 scalar `volatile` object/pointerのload/storeをtyped SSA→MIRで
        保持し、mem2regで除去・昇格させず、`-O2` verified backendのi686/AMD64で
        local／reference／indirect aggregate member／global accessを検証
  - [x] `restrict`をpointer自体へ保持し、object/incomplete typeを指す制約、
        function pointer・非pointer適用の明示diagnosticを型名・宣言・member・
        `sizeof`/cast経路と両arch回帰で検証
  - [x] nested pointer cv qualification conversionで、直接pointeeの修飾追加を
        維持しつつ、保護されていない内側levelの危険な修飾追加・破棄を拒否。
        const-protected intermediate pointerとi686/AMD64のdiagnosticを回帰検証
  - [x] built-in assignmentのRHSも初期化・引数・returnと同じ暗黙変換規則で
        検査し、const喪失・非互換pointer・異なるaggregate間の代入を拒否。
        整数／pointerの暗黙変換はCのゼロ整数定数式とC++のゼロ整数literal
        のみ許可し、C++ enum expressionとpointer→integer（bool以外）の
        暗黙変換は拒否。
        C17の列挙定数式を`int`型として扱う。
        `void*` と関数pointer間の暗黙変換、C++の`void*`からobject pointer
        への暗黙変換も拒否し、Cのobject pointer↔`void*`は維持。
        C17/C++20正例・負例をi686/AMD64で回帰検証
  - [x] typedef配列へ適用する`const`を要素型へ伝播し、要素の書き換えを拒否。
        配列typedefの`_Atomic` qualifierも明示的に診断し、RCCの`test-optimize`
        で正例・負例を検証
- [ ] VLA、compound literal、designator列・brace省略・上書きを含む初期化子の完全実装
  - [x] brace省略列でbraced／文字列aggregate節を一つのsubobjectとして
       扱い、その後のscalar節を次のsubobjectへ進める。未指定長の多次元
       配列bound推論と、static／automaticの両arch実行を検証
  - [x] C17のネストしたfield／index designatorの直後に続くscalar節を、
       外側aggregateではなく指定subobject内の次のscalarへ継続。static／
       automatic初期化をlegacy／verified backendと両arch object、x64実行で検証
  - [x] chained C field/index designatorの次節が内側arrayの末尾を越える場合、
       enclosing designated struct内の次scalar memberへdepth-first継続。途中の
       array element消費も含めstatic／automatic、両backend・両archで検証
  - [x] 直接field designatorの後続scalar節がaggregate subobjectへ進む場合、
       そのsubobjectをbrace-elision順にscalar leafへ分配してから外側の次memberへ
       継続。static／automatic、legacy／verified、両archとx64実行で検証
  - [x] chained designatorの次subobject自体がarray等のaggregateの場合も、
       brace-elisionでscalar leafを埋めてから同じpath上の次subobjectへ継続。
       2次元arrayのstatic／automatic、両backend・両archとGCC実行結果を照合
- [x] block-scope VLAの非定数境界、実体ポインタ・byte extent保存、添字、
      `sizeof`、i686/AMD64 dynamic stack allocation、通常のscope exitと
      `break`／`continue`／`return`／有効な`goto`でのstack reclamation
- [x] VLAスコープへ入る`goto`を診断し、VLA領域を跨ぐ有効な`goto`で
      必要なstack extentだけを復元
  - [x] Cの配列parameterをpointerへadjustし、variably modifiedな内側配列の
        bound検査と多次元index stride計算を保持
  - [x] parameter boundをfunction entry／VLA宣言時に一度だけ評価し、
        多次元strideと`sizeof`で保存extentを再利用
  - [ ] 全宣言形式
    - [x] for初期化宣言内のVLAとscalarの複数declaratorをlowerし、
          array lifetimeをloop全体へ保持。i686/AMD64で実行し、loop後の
          identifierがscope外として診断されることを検証
    - [x] block-scope variably modified typedefをloweringし、linkageを持つ
          variably modified objectとstruct/union memberを両archで診断
    - [x] VLA objectのstatic/thread storage durationを拒否しつつ、static
          pointer-to-VLA（object自体はVLAではない）を許可。extern VM objectの
          linkage診断と合わせ、i686/AMD64のpositive実行・negative診断および
          GCC C17 `-pedantic-errors`と照合。positive／negative host compiler
          checks（isolated static-VLA negativeを含む）を`test-vla-declarations`
          へ追加し、GCC/Clangの`test-c17`でgate。focused targetはWindows hostで
          GCCとMSYS2 Clangの双方でpass
    - [x] C17の旧式identifier-list function declaration/definitionを型付き
          parameter declarationへ接続し、既定int・array/function parameter
          adjustment・未知parameter診断と両arch compile/run回帰を追加
    - [x] 空parameter-list宣言と、default argument promotionで`int`となる
          enum parameterを持つprototypeの互換性を確認。RCCの両arch object生成・
          x64実行とGCC C17 `-pedantic-errors`を`test-c-old-style`で照合
  - [x] 一つのC17宣言文に複数のobject／pointer／array／function prototype／
        typedef declaratorを許可し、同一source scopeの個別Declへ分解して
        global/local initializerとpointer／array型を両archで実行・object回帰する
  - [x] C++の複数declarator宣言も同じscope-preserving経路で展開し、global/local
        object、pointer、array、function prototypeの混在をx64実行と両arch
        object回帰で検証する
  - [x] C++ `auto`／`decltype(auto)`の宣言initializerをassignment-expression
        境界で分離し、同一宣言文の複数推論declaratorを順序どおりscopeへ登録
        してx64実行・両arch object回帰を通す
  - [x] function prototype scopeの`[*]`を未指定VLAとして保持し、
        definition／local／typedef／type-nameでの誤用を診断
  - [x] type-nameの`sizeof(int[count])`でVLA boundを意味解析し、
        実行時byte extentを計算
  - [x] 配列parameterの`static`／`const`／`volatile`／`restrict`指定を保持し、
        調整後pointerへ反映。parameter以外の誤用とboundなし`static`を診断
  - [x] flexible array memberの構造体末尾・要素型・union制約を診断し、
        構造体初期化ではflexible memberを省略して扱う
  - [x] array/struct/unionの同一subobjectに対する後続initializer上書き
  - [x] brace省略列を型のsubobject順にnested initializerへ正規化し、
        braced／文字列aggregate節とscalar節の混在を含めて
        i686/AMD64のstatic/automatic storageへlowering
  - [x] compatible aggregateのlocal copy初期化と匿名struct/union member
  - [x] struct/unionの通常・連鎖代入と端数byteを含む両arch copy codegen
  - [x] automatic storageのscalar/array/aggregate compound literal
  - [x] i686/AMD64のaggregate returnと戻り値からのmember/argument連鎖
- [x] bounded `_Generic`、atomics、thread-local storage
  - [x] Lower the commonly used GCC compatibility builtins
          `__builtin_expect` and `__builtin_unreachable` as validated intrinsics
          on i686/AMD64, including real undefined-path trapping, i686 wide-
          scalar pair lowering, and C/C++ compile-and-run coverage.
  - [x] Lower `__builtin_trap` as a validated no-argument terminating
        intrinsic with real i686/AMD64 trap instructions and diagnostic
        coverage.
  - [x] Lower common scalar builtins `__builtin_bswap{16,32,64}` and
        `__builtin_{clz,ctz,popcount,parity,ffs}{,l,ll}` with target-width
        validation. The bit-count, parity, and first-set-bit operations use
        verified typed-SSA lowering where the target representation is
        available and direct x86/x86-64 lowering for the complete supported
        family, with deterministic zero handling, dual-arch emission, and
        x86_64 execution coverage. `__builtin_prefetch` is also lowered to
        validated x86 read/write prefetch hints with optional-argument
        defaults and dual-arch object/runtime coverage.
  - [x] Extend `__builtin_strlen` from string literals to character pointers
        and arrays. Evaluate the operand once and emit a real byte-scan loop
        in both legacy and verified typed-SSA i686/AMD64 paths, while retaining
        literal constant lowering and explicit diagnostics for non-character
        operands; cover C/C++ execution and verified object generation.
  - [x] Lower GCC-compatible `__builtin_choose_expr` by requiring an integer
        constant condition, type-checking both result expressions, and emitting
        only the selected expression so the unselected branch has no runtime
        side effects; cover legacy and verified typed-SSA C17/C++20 i686/AMD64
        emission, execution, and invalid-condition and arity diagnostics.
  - [x] Parse GCC-compatible `__builtin_types_compatible_p` type-name operands
        and fold typedef, pointer, array, function-prototype, nested-qualifier,
        and signedness comparisons
        to a target-independent integer constant with explicit non-type
        diagnostics; cover legacy and verified C17/C++20 i686/AMD64
        compilation and execution.
  - [x] `_Generic`のcompatible type選択、default、非評価control
  - [x] 8/16/32-bit整数atomic load/store/exchange/CAS/fetch add/sub/bitwiseとfull fenceの両arch codegen
  - [x] GCC互換のgeneric `__atomic_load`／`__atomic_store`／
        `__atomic_exchange`／`__atomic_compare_exchange`を、対応する
        lock-free integer/pointer object widthのresult／expected pointer形式として
        意味解析し、i686/AMD64の実atomic load/store/exchange/CASへlowerする。
        load/storeのmemory-order制約、C/C++コンパイル、i686/AMD64実行、
        および不正order／aggregate診断を回帰検証する。
  - [x] atomic-qualified整数の`&=`／`|=`／`^=`を一回評価のCAS retry loopでloweringし、
        i686/AMD64の8/16/32/64-bit幅で結果値と既存atomic API回帰を実行
  - [x] atomic-qualified整数の`*=`／`/=`／`%=`／`<<=`／`>>=`を一回評価のCAS retry loopで
        loweringし、i686の8/16/32-bitとAMD64の8/16/32/64-bit幅を実行検証。
  - [x] i686 64-bit atomic arithmetic／shift RMWも保存した右辺を一回だけ評価し、
        `CMPXCHG8B` retry loopと既存のlow-64 multiply／software divide／shift
        loweringへ接続して、signed／unsignedと右辺評価回数を実行検証
  - [x] i686 64-bit atomic pre/post increment／decrementを一回評価の
        `CMPXCHG8B` retry loopへ接続し、carry／borrowとprefix/postfix結果、
        添字式の評価回数を両archで実行検証
  - [x] i686 64-bit atomic `+=`／`-=`を一回評価の`CMPXCHG8B` retry loopへ
        接続し、64-bit carry／borrow、右辺評価回数、および競合時の
        failed-CAS一時スタック解放を実行検証
  - [x] i686/AMD64生成コードのnative実行と16/32-bit競合回帰
  - [x] 8/16/32-bit標準integer typedefと`atomic_flag`向け`<stdatomic.h>` API
  - [x] wide/pointer-sized型を含むC17標準atomic typedef全面とarch別lock-free定数
  - [x] 定数memory orderの範囲・load/store・CAS failure/weak制約diagnostic
  - [x] AMD64 64-bit整数atomic全操作と競合実行試験
  - [x] 両arch pointer atomic load/store/exchange/CASと不正RMW拒否
  - [x] i686 64-bit scalar ABI lowering
    - [x] EDX:EAX戻り値、8-byte cdecl引数、load/store、加減算、bitwise基礎
    - [x] signed/unsigned比較と0..63-bit shift
    - [x] low-64 multiplyとsigned/unsigned software divide/modulo
    - [x] 前置/後置increment/decrementと全integer compound assignment
    - [x] CMPXCHG8B load/store/exchange/CASと全RMW retry loop
- [ ] parser error recoveryとdiagnostic品質の網羅試験
  - [x] block parserの進捗保証と未知parameter/field型のNULL-safe回復
  - [x] `()`, `[]`, `{}`の深さを追跡してネスト内の誤った同期点を避け、
        C17/C++の宣言開始点と後続宣言の回復を両archでタイムアウト・
        クラッシュなしに検証
  - [x] missing terminatorでcursorがtypedef名、C++ `bool`/`char8_t`、
        `requires`等の後続宣言開始点に残った場合、そのtokenを先に消費せず
        次のparse iterationへ渡し、連続無進捗時だけ確実に同期消費する回復を
        C17/C++20の診断fixtureで検証
  - [x] malformed C++20 fixtureを`test-cxx`／`test-ci`の回帰gateへ含め、
        i686/AMD64の双方で10秒timeout、終了コード1、期待診断、診断上限未到達、
        object未生成を検証
- [x] C17 conformance compile-and-run suite

## 3. C++20 frontend / ABI

- [x] `rcc++` entrypointとC++20既定mode
- [x] 明示的C++関数の`return`値有無・void式・変換可能性をsemaで検証し、
      不正なreference/scalar returnを両archで診断、void式returnをhost実行で検証
- [x] 同一scopeのC++関数再宣言で、解決済み`noexcept`仕様の不一致を診断する。
      省略仕様と`noexcept(false)`、bare `noexcept`と`noexcept(true)`の互換例、
      両方向の不一致、およびconstant-expression specificationの差を、free関数と
      class memberの両方でi686/AMD64検証
- [x] C frontendと共通のtarget/preprocessor CLI
- [x] bounded class、継承、virtual dispatch実装
  - [x] 非static・非virtualメンバー関数の`this`引数、暗黙field参照、
        `obj.method`／`ptr->method`呼び出しと両arch実行
  - [x] C++ member `alignas` をクラスlayoutのalignment、padding、
        `alignof`へ伝播し、i686/AMD64のstatic_assert回帰で検証
  - [x] C++ class declaration `alignas` をクラス自身のalignment、size、
        包含クラスとclass template instantiationのpaddingへ伝播し、
        i686/AMD64のstatic_assert回帰で検証
  - [x] staticメンバー関数をqualified source lookup（`Class::func()`）と
        Itanium link nameへ分離し、暗黙`this`なしの直接・object経由・クラス内
        呼出しをi686/AMD64で実行検証
  - [x] 同一クラスのnon-static member overloadを引数変換順位とdefault
        argumentで選択し、`obj.method`／`ptr->method`を両archで実行検証
  - [x] accessibleな非virtual基底のメンバー関数を派生型からlookupし、
        記録済みbase subobject offsetで`this`を調整して`obj.method`／
        `ptr->method`を両archで実行検証
  - [x] primary vptr、class vtable、local/global vptr初期化、virtual
        callの間接分岐を実装し、overrideを含むi686/AMD64実行を検証
  - [ ] 標準C++の全class layout、特殊メンバー、virtual ABI互換性
    - [x] Resolve non-dependent qualified class-scope alias type-ids in
          ordinary declarations, including namespace-qualified and inherited
          aliases, concrete class-template and alias-template owners, optional
          `typename`, and nested `>>` closers. Verify private/ambiguous lookup
          diagnostics, i686/AMD64 output, PE-i386 objects, and AMD64 execution.
    - [x] Parse class-scope alias templates on concrete non-template classes
          and resolve both `Owner::Alias<T>` and `Owner::template Alias<T>` in
          ordinary qualified type-ids. Cover unqualified member use,
          namespace-qualified owners, optional `typename`, nested template
          closers, protected access from derived context (including the
          private-inheritance boundary), and private/protected diagnostics.
          `test-cxx-alias-templates` verifies i686/AMD64 output, AMD64
          execution, C++11 parsing/code generation, exact negative diagnostic
          counts, and no invalid object output.
    - [x] Resolve alias templates owned by concrete, explicit, and partial
          specializations of class templates; substitute enclosing type and
          integral non-type parameters with inner arguments and defaults. Also
          resolve a dependent owner through a type template parameter. Cover
          namespace-qualified owners, nested template closers, optional
          `typename`/`template`, private/protected diagnostics, more than 32
          inner parameters, i686/AMD64 output, PE-i386 objects, C++11 parsing,
          AMD64 execution, exact error counts, and no invalid object output in
          `test-cxx-alias-templates`.
    - [x] Resolve inherited alias templates through public, protected, and
          private base paths, including multi-level and specialized class-
          template bases; preserve access boundaries and diagnose ambiguous
          lookup. `test-cxx-alias-templates` verifies the valid cases on
          i686/AMD64, C++11/C++20 generation, PE-i386 output, AMD64 execution,
          and exact private/protected/ambiguous diagnostics with no invalid
          object output.
    - [x] Substitute dependent integral non-type arguments in class-template
          alias owners, retaining `Owner<N + 1>` and alias default `M = N`
          until the enclosing function-template instantiation. The alias-owner
          regression verifies C++11/C++20, i686/AMD64 output, PE-i386 objects,
          AMD64 execution, exact access diagnostics, and no invalid `.ro`;
          related non-type-template, function-reference, requires-type, and
          inherited dependent-alias regressions pass.
    - [x] Honor friend-class access for inherited alias templates: a base
          friend can reach that base's private alias through public
          inheritance, and a derived-class friend can use protected aliases
          even through private inheritance; a derived-class friend still cannot
          reach a private alias declared by a base. Cover namespace-qualified
          friendship, i686/AMD64 generation, C++11/C++20, AMD64 execution,
          exact negative diagnostics, and no invalid `.ro` in
          `test-cxx-alias-templates`.
    - [x] Preserve inherited-alias friendship while parsing out-of-class
          non-template and function-template friend bodies. Resolve grants by
          function signature in the shared parser, restore the surrounding
          access context after the body, and reject a same-name unrelated
          overload; match namespace-qualified friend declarations as well.
          Verify both target widths, host execution, exact negative diagnostics,
          and no invalid object output in `test-cxx-alias-templates`.
    - [x] Resolve private inherited aliases in out-of-class friend function
          return and parameter declarators. Parse under provisional namespace
          friend grants, then validate each recorded alias use against the
          completed ordinary or function-template signature before accepting
          it; unrelated same-name overloads remain rejected. Cover ordinary
          and function-template return/parameter aliases, private member
          access in a matching body, C++11/C++20, i686/AMD64 output, AMD64
          execution, exact two-error diagnostics per target, and no invalid
          object output in `test-cxx-alias-templates`.
    - [x] Validate inherited-alias access independently for each
          comma-separated function declarator sharing one declaration-specifier
          alias type. A matching friend followed by a same-type non-friend in
          the same declaration must diagnose the latter; `test-cxx-alias-
          templates` checks exact three-error counts and no invalid object on
          i686/AMD64, while the complete alias-template gate passes.
    - [x] Match alias-bearing friend function templates with the existing
          bounded template-head/constraint and `noexcept(expression)`
          equivalence logic, including renamed parameters; do not grant access
          to an otherwise same-type overload with a different constraint.
          `test-cxx-alias-templates` covers a matching constrained definition
          with renamed parameters and a dependent noexcept expression, AMD64
          execution, and a differing constraint rejected on both targets with
          no `.ro` output.
- [x] Treat class declarations in `extern "C"`/`extern "C++"` linkage blocks as
      C++ class declarations, and resolve elaborated `struct T` type specifiers
      back to the registered C++ class type; verify member access in both target
      widths and the verified SysV aggregate `va_arg` C++ translation unit.
- [x] Parse language-linkage specifications at namespace scope, register their
      declarations for namespace lookup, retain unmangled C function/data ABI
      names, and emit namespace-qualified C++ function/data ABI names; verify
      both target object symbols, in-namespace and qualified out-of-namespace
      calls, and class member access; link and execute the complete C/C++ symbol
      interaction on the x64 host.
- [x] bounded overload resolution、namespace、ADL、two-phase lookup
  - [x] target幅`nullptr_t`、`auto`保持、null-pointer conversion、条件式・overload
  - [x] 宣言側default argument、再宣言累積、overload viability、call-site補完
  - [x] parser-knownなnamespace所属class型の引数からqualified symbolをADLで
        解決し、free function callをi686/AMD64で実行検証
  - [x] class-templateの型引数、complete classのdirect/indirect base、
        pointer-to-memberのowner class、およびglobal-scope classから
        associated namespace/classを集め、型集合・overload集合を動的拡張。
        template-argument/base/member-pointer/global-scope由来のADLを
        i686/AMD64で実行検証
  - [x] integer user-defined literal operatorをItanium `li`名修飾へ接続し、
        built-in integer suffixを含むi686/AMD64の生成・実行を検証
  - [x] C++17 floating、character、string user-defined literal operatorを
        bounded scalar/pointer ABIへ接続し、`double`、`char/char8_t`、
        `const char*/size_t`の生成・実行を検証。long doubleおよび未対応署名は
        明示診断する
  - [x] bounded ordinary overload conversion rankingを引数ごとの優越関係へ
        更新し、浮動小数点promotion、pointer/nullptr-to-bool conversion、
        直交した変換列の曖昧性をi686/AMD64で回帰検証。テンプレート候補の
        全partial orderingと標準の全conversion rankは未完了
  - [x] publicな非virtual/virtual派生クラスlvalueを基底クラス参照へ束縛する
        標準変換、vbptrを含む参照引数のsubobject調整、およびDerived&が
        Base&より優先されるbounded overload選択を両archで回帰検証
  - [x] 非virtual多重継承で同一基底型への複数public経路を曖昧変換として
        拒否し、pointer/reference引数の両arch診断を回帰検証
  - [x] bounded function-template overload candidatesで固定・非依存関数
        parameterの標準変換をdeduction後のviabilityへ分離し、候補間の
        conversion vectorを引数ごとに比較。直交したテンプレート候補の
        ambiguityと既存の両arch実行を回帰検証
  - [x] publicなunique non-virtual derived-to-base class-by-value変換を追加。
        trivially-copyable baseのコピー、非ゼロoffsetを持つ多重継承、
        template/non-templateの近いbase優先、private/複数経路の拒否を
        検証。非trivialなderived prvalue引数は呼び出し後にcomplete objectを
        一度だけ破棄し、verified SSAのi686/AMD64両方でfallbackなし・実行確認
        する。`test-cxx-function-template-overloads` と
        `test-verified-cxx-temporary-cleanup` を参照
  - [x] trivially-copyable baseへのpublicでuniqueなclass-by-value変換を
        nested virtual base経由にも拡張。全public/private/virtual経路を数えて
        accessとambiguityを判定し、vbptrとvirtual base内の固定offsetから
        実際のsubobjectをコピーする。nontrivial derived prvalue引数は
        complete objectを保持してcall後に一度だけ破棄する。通常backendは
        i686/AMD64生成とx64実行、GCC `-fno-elide-constructors`比較を確認。
        verified backendでlocal objectのvptr/vbptr初期化も行い、両archで
        5関数・fallbackなし、およびi686/AMD64 objectの実行を確認。
        object runnerは`.text`と`.rodata`のrelocationを適用してvtable経由の
        virtual dispatchも実行する。
        `test-cxx-function-template-overloads` と
        `test-verified-cxx-virtual-base-value` を参照
  - [x] verified backendで、noexcept空constructor・empty trivial virtual base・
        1個のpointer memberを1個のpointer引数から初期化するderived prvalueの
        virtual-base-to-value call argument conversionをlowerし、complete objectの
        destructorがcall後に一度だけ実行されることをi686/AMD64で確認。
        `test-verified-cxx-virtual-base-value` はfallbackなしで両objectを実行する。
  - [x] 上記constructor loweringを任意個数のscalar member/parameterへ拡張。
        constructor引数を一度ずつ評価し、member initializerから重複のない対応
        parameterへmapする。virtual-base prvalue cleanup regressionを4 member/5
        argumentへ拡張し、1個の未使用scalar parameterを含む5 argumentすべての
        一回評価を副作用helperで確認する。
        宣言順と異なるparameter/field対応を重み付きdestructorで識別する。
        destructorが全fieldを読み、ローカルcounterへ反映することをi686/AMD64の
        verified object実行でfallbackなしに確認。
  - [x] bounded empty-body/noexcept constructorでmember initializerに使われない
        scalar parameterも保持し、通常x86 codegen双方で対応するcall argumentを
        一度だけ評価する。focused targetのverified i686/AMD64 object実行と通常
        AMD64 host実行が成功。
  - [x] bounded constructorの`float`/`double` memberをverified backendで初期化し、
        double storageのzero-initを追加。verified backendはconstructor parameter
        からmember型へ数値castして格納し、通常i686/AMD64 codegenも同じint-to-double
        fixtureを生成する。verified i686/AMD64 objectはfallback 0で実行成功し、
        i686 COFFもassemble、通常AMD64 hostでも成功。同じfixtureで`float`/`double`
        aggregateのvalue-initializationがゼロになることも実行確認。
  - [x] local class constructor call loweringでSSA value id `0`を有効値として
        扱い、`RCC_IR_VALUE_NONE`だけを無効値とする。scalar memberをparameterで
        初期化し、non-empty `noexcept` bodyでscalar演算・代入するfixtureを追加。
        `test-verified-cxx-reference-local` はi686/AMD64 `-O2`で8関数・fallback 0、
        両verified object実行と通常AMD64 host実行に成功。
  - [x] verified backendで複数scalar member/parameterを持つconstructor、
        arithmetic/member assignmentを行うnon-empty `noexcept` body、複数の
        nontrivial direct non-virtual base constructor、複数member/parameterの
        initializer式とcall initializer、さらにbase/array/nested aggregateを含む
        trivial copyを実装・回帰検証。constructor body内のscalar local、if/else、
        for loop、switchのcase/default/breakを検証済み。
        `test-verified-cxx-reference-local` はi686/AMD64の`-O0`/`-O2`で各25関数・
        fallback 0、4 verified object実行、PE-i386生成とlegacy AMD64実行を通過。
  - [x] Base constructor callsが明示的に`noexcept`で、derived constructorのbodyが空、
        field initializerがなく、own data memberがscalarのみの場合、constructor自身に
        `noexcept`指定がなくてもtyped SSAでlowerする。`VerifiedDerived`から`noexcept`を
        外し、i686/AMD64の`-O0`/`-O2`で25関数・fallback 0、object実行とlegacy AMD64
        executionを`test-verified-cxx-reference-local`で検証。
        `test-verified-cxx-throwing-temporary`も成功。
  - [x] verified SSAで唯一のdirect non-virtual baseがpotentially-throwing constructorを持つ
        bounded caseをlowerする。derived bodyは空、subobject fieldはscalar、base call引数と
        後続scalar member initializerはnon-throwingと証明できる場合だけ受理し、base throw時に
        未完成のderived subobject cleanupを誤登録しない。実throw/catchと成功値をx64で実行し、
        i686/AMD64の`-O0`/`-O2`で41関数・fallback 0を確認。`test-verified-cxx-throwing-temporary`
        に統合し、legacy x64 executionでもthrow propagationを検証。
  - [x] bounded constructorで複数の明示direct non-virtual base initializerをlowerし、
        各baseの完了後にunwind cleanupを登録する。後続baseがthrowした場合、先行する
        完成済みbaseをreverse orderで破棄する。derived body内のnested/fallthrough switchを
        通常のstatement CFGへlowerし、body throwでも完成済みbaseをreverse orderでcleanup。
        direct baseはnon-virtual leafに限定。derived memberはscalarまたはscalar-fieldを持つ
        non-virtual leaf classとし、scalar default-member-initializerの直接call、およびmember
        constructorのthrow後も完了済みsubobjectをreverse orderでcleanupする。潜在的にthrowする
        initializer callの引数はnon-throwingでtemporary cleanup不要の形に限定。fixtureの47関数
        すべてがi686/AMD64の`-O0`/`-O2`でfallback 0となり、second-base/member-constructor throw、
        switch fallthrough、body/loop/DMI throw、successful/member/body cleanup order、
        完成後caller throw、first-base throwを実行検証。
        `test-verified-cxx-throwing-temporary`と、この拡張を含む
        `test-verified-backend`が成功。基底cleanup実装は`57f2d8d`、nested/fallthrough
        switchとbody-throwへの拡張は`6646bb8`。throwing scalar DMI拡張は`88e0aff`。
        throwing non-trivial leaf member constructorと完成memberのcleanup検証は本変更。
  - [ ] broader virtual-base constructor chains/cleanup、nontrivial/deleted target
        copy constructor、call引数以外のfull-expression cleanupと未検証の例外経路が残る。
        標準conversion subsequenceと
        pointer-to-memberを含む全rank/partial orderingも未完了
  - [x] Namespace extension compatibility: verify reopened named and
        translation-unit anonymous namespaces, lookup through the transitive
        inline-namespace set for both namespace definitions and qualified
        namespace lookup, rejection of changing an ordinary namespace to
        inline, and namespace-alias/ambiguous-name conflicts against the C++
        standard. Named and translation-unit anonymous namespace definitions
        share owners; namespace-definition and qualified lookup traverse
        transitive inline sets and reject ambiguous results. Alias declarations
        conflict with direct declarations, while an alias may coexist with a
        same-named declaration injected from an inline namespace; later
        qualified lookup is diagnosed as ambiguous. The focused
        `test-cxx-namespace-extension` target passes on i686 and AMD64, executes
        the valid fixture, and checks rejected cases against GCC C++20.
  - [ ] 標準C++の全conversion rank、ADL、two-phase lookup互換性
- [x] C++ enumのfixed underlying typeを保持し、signed/unsigned各幅の明示列挙子と
      暗黙の次値が表現可能か検査。i686/AMD64で境界値を実行し、範囲外を診断する。
- [x] Preserve the direct fixed-enum underlying conversion for overload ranking,
      while applying the promoted underlying integer type to built-in unary
      minus/complement, shift, arithmetic, and constant-expression operations.
      Regress an `unsigned char` unscoped enum at its maximum with compile-time
      result-type/value assertions and AMD64 runtime checks.
      `test-cxx-enum-class` passes on i686/AMD64 and validates generated `.rin`,
      `.rll`, and `.drv` images. The C++ scalar variadic-promotion
      interoperability gate also passes.
- [x] Model unary `+` as an expression so C integer promotions and C++ built-in
      conversions, overloads, and template substitution retain their semantics.
      `test-integer-promotions` covers C `_Generic`, diagnostics, i686/AMD64
      execution, and AMD64 `-O2`. `test-cxx-enum-class` covers bool/floating and
      enum promotions, member/free overloads, dependent templates, implicit and
      ambiguous/explicit class conversions, both architectures, optimized host
      execution, verified-backend `-O2` on both targets, and validated `.rin`,
      `.rll`, and `.drv` output.
- [x] fixed-underlying enumでunsigned 64-bit列挙子を`ULLONG_MAX`まで保持し、暗黙の
      次値／overflowとtyped constant evaluationをi686/AMD64で検証。DWARFにも
      `DW_ATE_unsigned`と`DW_FORM_udata`でunsigned値を出力し両archで検証する
- [x] underlying type未指定C++ enumのimplementation-defined選択を、
      `int`→`unsigned int`→target `long`→`unsigned long`→`long long`→
      `unsigned long long`の順で全列挙値を表現できる最初の型へ確定し、
      enum閉じ括弧前の列挙子expression type／暗黙増分の型遷移、閉じ括弧後の
      enum型、算術昇格、constexpr、DWARF encoding/valueへ反映。i686/AMD64で
      signed/unsigned/wide境界、ULLONG_MAX、型なしで表せない混合値を検証
- [x] Emit C/C++ enumeration DIEs with `DW_AT_type` references to their actual
      integer underlying type, preserve the referenced base type's byte size
      and signedness, remove the stray non-standard enum encoding byte, and
      verify C, fixed/inferred unsigned C++, and signed C++ enums on i686/AMD64.
- [x] Mark scoped `enum class`/`enum struct` DIEs with the DWARF 4
      `DW_AT_enum_class` flag and verify the flag, underlying base type, and
      enumerator value on i686/AMD64.
- [x] Inferred/unscoped C++ enum coverage emits direct unsigned-v3 `.rin`, `.rll`,
      and `.drv` images on i686/AMD64 and validates each artifact with rinvalidate.
- [x] bounded templates、concepts、constexpr/consteval、lambda
  - [x] bounded type/non-type parameter packs、pack expansion、fold expression、
        empty-pack identity、pack-based static membersの両arch回帰
  - [x] C++20 named conceptを`template<Concept T>`制約付きtype parameterへ
        適用し、instantiation時のconcept評価とC++17以前のstandard gateを検証
  - [x] Named concept-idを通常のbool式として評価し、non-type/type argumentの
        positive・negative実行例を追加。`test-cxx-named-concepts`と全`test-cxx`
        gateを完走し、GCC C++20 oracle・i686/AMD64 codegen・AMD64実行を確認
  - [x] Template function body内のdependent named concept-idをinstantiation
        まで保持し、non-type/type argumentの正負例を`test-cxx-named-concepts`
        で検証。GCC C++20 syntax、i686/AMD64生成、AMD64実行が通過
  - [x] parser-known型によるdirect/pointer function-template deductionと
        trailing type defaultの実体化
  - [x] 先行非型引数を参照する整数constant-expression defaultの評価
  - [x] dependent aggregate templateの未対応partial loweringを診断
  - [x] C++20 aggregate parenthesized initializationを完全なpublic struct
        aggregateと固定長配列へloweringし、static/automatic storageの両arch
        実行、過剰initializer診断、C++17以前の明示的standard-gateを検証
  - [x] C++17 aggregateのpublic non-virtual direct baseを先に、宣言順に
        memberを初期化し、複数基底・static/automatic storage・C++20
        parenthesized initialization（base object expressionsを明示）を
        両targetで生成/x64実行。C++14とparen brace-elisionは両targetで診断
  - [x] C++ direct/list aggregate初期化で`constexpr`/`inline`/`constinit`
        宣言属性を保持し、most-vexing parseの関数宣言を誤認せず、C++20
        designated/nested initializerとconstexpr aggregate member accessを
        共通initializer/sema経路でi686/AMD64の全C++回帰まで検証
  - [x] bounded class-template partial specialization matching and
        per-argument partial ordering for pointer patterns, cv-qualified
        pointer patterns, cv-sensitive explicit and exact integral patterns,
        mixed type/non-type patterns, requires-clause viability,
        unsupported-constraint diagnostics, and orthogonal ambiguity
        diagnostics with dual-architecture regression coverage
  - [x] Among otherwise equivalent function-template candidates, prefer the
        overload with more fixed function parameters and prefer a non-pack
        signature over a trailing function-parameter pack when fixed prefixes
        tie. GCC C++20 reference checks and i686/AMD64 generated execution are
        included in `test-cxx-function-template-overloads`.
  - [x] Cover C++20 pack partial ordering where `g(T*, U...)` beats `g(T)` for
        a pointer argument, and where `choose(T*, Rest...)` beats an all-pack
        `choose(T...)` for a pointer followed by another argument. GCC C++20,
        RCC++ i686/AMD64 generation, and AMD64 execution pass in
        `test-cxx-function-template-overloads`.
  - [x] Rank constrained overloads by named-concept subsumption: select a
        concept that adds a requirement over a shared named concept, and keep
        distinct, non-subsuming atomic constraints ambiguous even when both
        evaluate true. GCC C++20, RCC++ dual-architecture diagnostics, and
        AMD64 execution pass in `test-cxx-function-template-overloads`.
  - [x] Compare original function-template array-reference patterns when one
        uses a direct non-type extent parameter and the other a fixed extent.
        For `int[1]`, `T (&)[1]` now wins over `T (&)[N]`; GCC C++20, RCC++
        i686/AMD64 generation, AMD64 execution, and the full native-Windows
        `test-cxx` aggregate pass.
  - [x] Deduce and order function-template candidates whose class-template
        parameter expands an integral non-type pack after a fixed prefix.
        Read the actual sequence from the class's expanded NTTP-pack storage,
        deduce the fixed prefix before the remainder, and keep per-element
        transformed-template pack mappings independent. Empty, one-element,
        and multi-element cases agree with GCC; RCC i686/AMD64 generated
        execution and the native-Windows overload regression target pass.
  - [x] Order direct class template-template pack expansions against a fixed
        leading template argument. The all-pack candidate handles an empty
        sequence; the fixed-first candidate wins for one and multiple
        template arguments. GCC C++20 and RCC i686/AMD64 runtime selections
        agree in the native-Windows overload regression target.
  - [ ] full partial ordering, all parameter-pack deduction rules, and
        unsupported constexpr evaluation required for complete standard
        conformance
- [ ] C++20 modules、coroutines
- [ ] Complete ordinary non-template rvalue-reference binding and value-category
      semantics. Named xvalue local binding and alias-preserving `int&`/`int&&`
      function returns now have regressions that compile for both targets and
      execute on the x64 host. Source-level reference collapsing and the public
      conversion-function cases returning class `T&`, `T&&`, or class prvalues
      are covered by `test-cxx-function-template-references` on both targets
      and the x64 host. Equal-rank non-template overloads now prefer `const T&&`
      over `const T&` for prvalues/xvalues and retain `const T&` for lvalues;
      GCC C++20, both target codegens, x64 execution, and the complete native
      Windows `test-cxx` gate pass. Broader call/return ABI interactions still
      need systematic coverage; a direct thread-local class-prvalue reference
      has dedicated per-thread host coverage below.
  - [x] Reject non-template `int&` returns from prvalues and `int&&` returns
        from lvalues as hard C++ semantic errors; verify both cases, invalid
        reference initializers, and invalid ordinary calls for i686/AMD64.
  - [x] Exercise direct non-template `T&` and `T&&` function returns in the
        verified SSA backend. GCC C++20 syntax checking, fallback-free i686 and
        AMD64 `-O2` object generation, and execution of both objects verify
        returned-address identity through mutation.
  - [x] Preserve a function lvalue reference through a non-template identity
        function and call the returned function. GCC C++20 syntax checking,
        i686/AMD64 code generation, and x64 host execution pass in
        `test-cxx-function-template-references`. The full native-Windows
        `mingw32-make SHELL=cmd.exe test-cxx` aggregate also passes (exit 0,
        2026-10-27).
  - [x] Apply fixed derived-to-base adjustment to direct non-template
        lvalue-reference returns, including `const` references, after lvalue
        address generation in both backends. A multiple-inheritance fixture
        places the returned public base at a nonzero offset; the regression
        checks pointer identity, alias mutation, direct xvalue returns, and
        indirect callback returns on i686/AMD64 generated host executables.
  - [x] Parse namespace and class-scope function-pointer alias type-ids with
        reference return/parameter types, then preserve `int&`/`int&&` return
        categories through indirect calls. GCC C++20 accepts the fixture;
        `test-cxx-function-template-references` verifies i686/AMD64 objects,
        PE-i386 output, and AMD64 host execution.
  - [x] Resolve direct `obj.callback(args)` and `ptr->callback(args)` as
        indirect calls through class data members declared with function-pointer
        aliases, including inherited fields and lvalue/rvalue-reference returns.
        GCC C++20 syntax checking and `test-cxx-function-template-references`
        verify i686/AMD64 code generation, PE-i386 output, and AMD64 execution.
  - [x] Verify public implicit conversion functions returning class lvalue
        references, rvalue references, and class prvalues during class-reference
        binding, including derived-to-base adjustment and lifetime cleanup.
        Sema lowers these conversion candidates, preserves the result category,
        and applies public derived-to-base adjustment. Codegen now materializes
        a class-prvalue receiver for the generated implicit object argument.
        `test-cxx-function-template-references` passes i686/AMD64 generation,
        x64 execution, cleanup-order checks, and invalid overload diagnostics.
  - [x] Support class-prvalue reference arguments with full-expression
        temporary storage and cleanup on both target backends. Call-result and
        compound temporaries, const-lvalue/rvalue references, derived-to-base
        binding, conditional/comma arguments, statement/condition/return
        boundaries, and destructor order are covered by
        `test-cxx-function-template-references`.
  - [x] Verify automatic lifetime extension when a direct class prvalue binds
        to mutable `T&&` and `const T&&` locals. Both objects remain alive
        across a subsequent call and are destroyed exactly once at scope exit;
        GCC C++20, i686/AMD64 generation, and x64 execution pass in
        `test-cxx-function-template-references`.
  - [x] Verify nontrivial class-prvalue member-receiver lifetime and destructor
        order for both function-return and compound receivers. The regression
        checks method-before-destructor ordering and exactly-once destruction,
        generates i686/AMD64 code, and executes on x64 in
        `test-cxx-function-template-references`.
  - [x] Support static-duration reference temporaries, including initialization
        and destructor registration for namespace and block static storage.
        The implementation covers scalar and aggregate temporaries on i686 and
        AMD64, uses guarded initialization for block statics, registers
        nontrivial cleanup with `__cxa_atexit`, and emits guard-abort cleanup
        for exceptions in a protected scope. The dedicated regression passed
        i686 PE object generation and x64 host execution, checking scalar
        initialization exactly once and reverse-order exactly-once class
        destruction. The earlier `test-cxx` suite passed; after adding the
        static subobject cases, the three focused static-reference targets
        pass independently. RinOS runtime integration remains unverified.
  - [x] Run `test-cxx-static-reference-retry` under the POSIX host harness for
        both i686 and AMD64: the first block-static reference initialization
        throws and is caught, `__cxa_guard_abort` is observed exactly once, a
        later attempt succeeds, and subsequent access reuses the same object.
        The generated target code executed successfully with the test exception
        and guard runtime; RinOS runtime integration remains separate.
  - [x] Support dynamically initialized non-TLS block-scope C++ integer,
        enum, and floating scalar statics in legacy and verified backends.
        Emit a per-object guard, perform first-use initialization, register
        `__cxa_guard_abort` for exception retry, then release on success. The
        regression throws on the first attempt, catches it, retries, and checks
        exactly-once reuse against GCC. Coverage includes integer, enum,
        floating, pointer, and volatile scalars; x64 legacy and verified
        execution pass at `-O0` and `-O2`, and all twelve functions emit without
        verified-backend fallback for i686/AMD64 at both optimization levels.
        aggregate and object-destruction cases remain separate.
  - [x] Support dynamically initialized function-local C++ `thread_local`
        integer and pointer scalars in legacy and verified backends. Keep each
        guard and object in TLS, register guard-abort cleanup before evaluating
        the initializer, and release only after success. GCC and RCC-generated
        x64 host code verify throw/abort/retry/reuse in two fresh pthreads;
        pointer checks confirm each thread retains its own TLS address. Legacy
        i686/AMD64 objects and verified SSA at `-O0`/`-O2` emit all TLS
        guard/value relocations and exception runtime imports without fallback.
        RinOS-target runtime integration and aggregate TLS initialization remain
        separate.
  - [x] Extend reference-initialization retry coverage to a nontrivial class
        prvalue whose constructor throws once. Verify guard abort, successful
        retry, stable reference identity, one `__cxa_atexit` registration, and
        process-exit finalization exactly once by executing generated AMD64
        code on Windows. i686 remains covered by target-object generation.
  - [x] Cover ordinary array lvalue/rvalue-reference parameters and returns,
        including xvalue casts, alias-preserving mutation, and the same
        reference category through a class-held function-pointer callback.
        GCC C++20 pedantic syntax checking, i686/AMD64 code generation, and
        AMD64 host execution pass in `test-cxx-function-template-references`.
  - [ ] Finish thread-local reference temporary acceptance. The i686/AMD64
        backends now emit per-thread owner/guard storage and register cleanup
        with `__cxa_thread_atexit`; libc start-return and `pthread_exit` paths
        run `__rin_cxa_thread_finalize` before `SYS_THREAD_EXIT`, ahead of POSIX
        TSD destructors. Host thread-exit regressions exist. Keep open until
        RinOS target execution and broader initializer/lifetime forms are
        verified.
  - [x] Cover a namespace-scope thread-local reference temporary initialized
        by a built-in comma expression. The POSIX regression checks one left-
        operand evaluation per thread, stable repeated reference identity,
        retained per-thread value, and exactly-once reverse destruction; GCC/
        Clang C++20 syntax checks plus RCC i686/AMD64 generation and x64
        pthread execution pass in WSL.
  - [x] Extend the TLS fixture with a function-local block-scope
        `static thread_local const Base&` bound to a class prvalue. Repeated
        calls with different initializer values verify the generated per-thread
        guard path and retain the referent after the helper's block exits.
        GCC/Clang C++20 syntax checks and RCC i686/AMD64 assembly and legacy
        `.ro` generation pass; the POSIX assembly gate now checks the helper is
        present on both targets.
  - [x] Run the POSIX pthread host assertions for the new block-scope case,
        including one-time per-thread initialization despite a changed second
        argument, stable per-thread reference identity, and thread-exit cleanup
        order. `make test-cxx-static-reference-temporaries-posix` passes in WSL
        with i686/AMD64 ELF assembly and the AMD64 pthread host assertions.
        Native Windows cannot execute this ELF-TLS host target; RinOS runtime
        integration remains a separate open item.
  - [x] Cover a block-scope `thread_local const Base&` initialized by a
        conditional selecting between derived-class prvalues. GCC C++20
        pedantic syntax checking, RCC i686/AMD64 object generation, and x64
        pthread execution verify that the first branch initializes once, the
        referent survives to thread exit, and derived-before-base cleanup is
        exact without signed overflow in the test's ordering counter.
  - [x] Verify namespace-scope `thread_local const T&` bindings to direct and
        derived-to-base class prvalues: i686/AMD64 generation must emit
        `__cxa_thread_atexit`, repeated access in one thread must reuse its
        owner, distinct threads must get distinct values, and thread exit must
        destroy the complete derived object exactly once in derived-before-base
        order. `test-cxx-static-reference-temporaries-posix` passes on
        i686/AMD64 object generation and x64 pthread host execution.
  - [x] Verify a namespace-scope TLS `const Base&` initialized through a
        user-defined conversion returning a `Derived` prvalue: run the
        conversion once per thread, retain each thread's complete result, and
        destroy derived then base exactly once at thread exit. The regression
        exposed and fixed missing caller-frame planning for guarded TLS
        initializer expressions; dual-architecture generation and x64 pthread
        execution pass in `test-cxx-static-reference-temporaries-posix` and
        `make CC=gcc test-ci`.
  - [x] Verify an automatic-duration `const Base&` bound through the same
        conversion retains the complete `Derived` until scope exit and destroys
        it derived-before-base. Both target objects compile and x64 pthread
        execution checks cleanup before TLS destructors in
        `test-cxx-static-reference-temporaries-posix`.
  - [x] Extend static-duration lifetime through direct member subobjects and
        explicit derived-to-base xvalue bindings, including virtual bases and
        class-prvalue sources selected by comma/conditional expressions.
        `test-cxx-static-reference-subobjects` verifies full-object destructor
        order, exactly-once cleanup, and source-expression side effects on
        i686/AMD64 generation and x64 host execution.
  - [x] Extend static-duration reference lifetime through user-defined
        conversion functions returning class prvalues. Global and block-static
        bindings verify conversion count, retained values, stable local address,
        and reverse exactly-once destruction in
        `test-cxx-static-reference-conversions` with i686/AMD64 generation and
        x64 host execution.
  - [ ] Extend static-duration reference lifetime through
        pointer-to-member-selected data subobjects. The public non-bit-field
        data-member-pointer path covers `T C::*`, `&C::member`, `.*`/`->*`,
        lvalue/xvalue selection, assignment, null values, implicit and explicit
        non-virtual owner conversion, and application through one public virtual
        base. `test-cxx-member-pointer-data` generates i686/AMD64 objects,
        verifies all seven typed-IR functions on both targets, and executes the
        generated AMD64 program on the host. Global and block-static reference
        bindings through a side-effecting member-pointer selector applied to a
        base subobject of a derived temporary verify retained values, one-time
        selector evaluation, and derived-before-base destruction exactly once.
        Native-Windows and POSIX generated AMD64 execution plus i686/AMD64 object
        generation pass in `test-cxx-member-pointer-reference-lifetime`; the
        POSIX `test-cxx` alias now reaches its explicit recipe instead of silently
        skipping this phony target. Keep this open until RinOS runtime integration
        is covered.
  - [x] Add global and block-static reference regressions where a side-effecting
        pointer-to-member selector applies a base-class member to a derived
        temporary. Verify selected values, one-time selection, stable local
        identity, and derived-before-base destruction on both host adapters.
  - [x] Extend TLS reference-temporary coverage to a derived owner whose base
        data member is selected by a side-effecting pointer-to-member function.
        `test-cxx-static-reference-temporaries-posix` verifies one selection per
        thread, stable reference identity and value, and exactly-once base and
        derived construction/destruction after pthread exit. GCC C++20 pedantic
        syntax checking, i686/AMD64 object generation, and POSIX host execution
        pass on WSL; RinOS target-runtime integration remains open.
  - [x] Add the missing POSIX test recipe and alias for
        `test-cxx-member-pointer-reference-lifetime`; ensure the existing C++20
        regression aggregate executes it instead of treating the phony name as
        an already-satisfied target.
  - [x] Complete remaining data-member-pointer conversions and contexts:
        hidden inherited data-member lookup, non-public inherited data members,
        and remaining access contexts. Unique public inherited data members now
        form pointers whose owner is the class that declared the member,
        including public virtual bases; positive generation/execution coverage
        and negative checks for private formation, ambiguous object paths, and
        ambiguous owner conversion run in `test-cxx-member-pointer-data` on
        both targets. Conversions across virtual bases are ill-formed under C++
        `[conv.mem]`; preserve diagnostics for them instead of treating them as
        an implementation feature. The full `test-cxx-member-pointer-data`
        gate passes after fixing aggregate copy assignment from an xvalue: both
        native backends now unwrap the reference's aggregate type and copy from
        the source object's address. The gate covers i686/AMD64 generation,
        seven verified-IR functions on both targets, generated x64 execution,
        GCC C++20, and expected access/ambiguity diagnostics.
    - [x] Check non-public inherited-member pointer formation in friend
          contexts against the declaring access and the C++ protected
          designating-class rule. Cover a base friend naming an inherited
          private member, a derived-class friend naming an inherited protected
          member, and rejection of `&Base::protected_member` from a derived
          member on i686/AMD64; `test-cxx-member-pointer-data` also executes
          both positive cases on the host.
    - [x] Let a directly declared derived member function hide a same-named
          inherited data member during `&Derived::name` lookup; check the
          member-function pointer is invoked (not the hidden base field) on the
          host and compile both target architectures against GCC C++20.
    - [x] Select a directly declared derived data field over an inherited
          same-named field during `&Derived::name`; execute both pointers and
          verify the base and derived subobjects retain distinct values on the
          host after i686/AMD64 generation and GCC C++20 acceptance.
    - [x] Preserve access granted to an ordinary non-template friend free
          function from its in-class declaration through the matching
          out-of-class definition. Verify direct private access, inherited
          protected object/member-pointer access, rejection of invalid
          protected designators, and rejection of a derived friend accessing a
          base-private member on i686/AMD64 plus host execution.
    - [x] Reuse a namespace-scope class identity from its forward declaration
          through the later definition, and keep repeated forward declarations
          (including one after completion) attached to that identity. Verify a
          free function declared as a friend by two classes can access both
          private members from its later definition; GCC accepts, and the RCC
          `test-cxx-member-pointer-data` and full `test-cxx` gates pass.
    - [x] Complete inherited name lookup for hidden/ambiguous paths and the
          remaining inheritance/access combinations. Direct hiding and
          non-field names, inherited field/function ambiguity, repeated
          non-virtual subobjects, `using` disambiguation/re-exposure (including
          transitive `using`), shared virtual bases, and public/protected/private
          inheritance access are covered. Inline inherited lookup is exercised
          in a member body. `make SHELL=cmd.exe
          RCXX_TARGET=build/namespace-rcc++.exe
          LDFLAGS=build/debug-inline-compat.o test-cxx-member-pointer-data`
          passed, including i686/AMD64 generation, verified IR, generated x64
          and GCC C++20 host execution, and expected ambiguity/access errors.
          The temporary linker compatibility object only bridges an unrelated
          in-progress debug-inline API edit in the shared checkout.
    - [x] Complete direct-field lookup from an in-class inline member body in
          non-template classes. Pointer formation is deferred until the class's
          completed layout and lookup set, covering private fields declared
          before and after the body. The focused i686/AMD64 gate, nonzero-offset
          generated x64 host execution, and GCC C++20 host execution pass.
    - [x] Carry inline data-member-pointer designators through class-template
          member cloning and specialization. Resolve the field and its final
          offset against the concrete specialization before semantic analysis;
          substitute the member-pointer owner in cloned `auto` declarations as
          well. `test-cxx-member-pointer-data` covers `int` and `double`
          specializations with distinct offsets and passes i686/AMD64
          generation, generated x64 host execution, and GCC C++20.
    - [x] Parse an in-class-defined friend function template as a namespace
          template and propagate its granting classes to instances. Merge an
          identical unconstrained type-parameter declaration with its later
          namespace definition and with matching friend declarations from a
          second class. Test private writes through both grants, external
          definition lookup, and unauthorized access rejection. The
          i686/AMD64 `test-cxx-friend-function` gate passes, and GCC C++20
          accepts the positive fixture.
    - [x] Diagnose a duplicate definition for a matching in-class friend
          function template when its namespace-scope definition is repeated;
          the diagnostic is checked for i686 and AMD64.
    - [x] Keep an in-class friend function template hidden from ordinary and
          qualified namespace lookup until a matching namespace-scope
          declaration is present. Admit it through bounded ADL only when an
          argument has the granting class, a derived class whose base chain
          includes it, or a class carried by a class-template type argument.
          Verify direct, derived, and template-argument ADL; reject unrelated
          unqualified, qualified, and unrelated-template-argument calls; and
          verify that a later namespace definition restores ordinary lookup
          without granting access to unrelated specializations. Traverse
          nested associated types without an artificial depth cutoff; a
          36-level class-template argument chain is covered. The i686/AMD64
          friend-function gate and GCC C++20 comparison pass.
    - [x] Merge defaults for matching type and bounded integral non-type
          template parameters across friend and namespace-scope redeclarations,
          including `U = T` and `M = N + 2` with renamed parameters. Preserve
          earlier defaults when the friend is later made visible; allow a
          default to be introduced only after an intervening namespace-scope
          declaration without one. Reject defaults on a non-defining friend,
          on the first namespace declaration after a hidden friend, and
          repeated type or integral non-type defaults. The i686/AMD64
          friend-function gate passes with x64 host execution; GCC C++20
          accepts the positive fixture and rejects all four invalid fixtures.
    - [x] Match friend function-template redeclarations with equivalent
          one-type-parameter template-template signatures and merge their
          defaults. Verify an earlier and a later `Box` default, instantiate
          `Box<T>` from the function body, retain the default when only the
          first type argument is explicit, and diagnose a repeated template
          default. The i686/AMD64 friend-function gate passes with x64 host
          execution; GCC C++20 accepts the positive fixture and rejects all
          five invalid fixtures.
    - [x] Match constrained friend-template redeclarations across simple,
          type, nested, and compound requirements in source order. Map renamed
          requires-expression parameters by nested scope and position, including
          a nested requires-expression that refers to an outer local parameter.
          Execute the positive friend call and reject a declaration whose
          requirement order differs on i686/AMD64; the focused friend gate and
          GCC C++20 comparison pass. Verify compound return-type matching with
          `std::same_as<int>` and reject a mismatched `std::same_as<long>` friend
          declaration as a separate ambiguous overload on both targets.
    - [x] Preserve the enclosing template scope after parsing a friend
          function-template declaration. Verify a simple requirement that calls
          an in-class-defined member function across friend/namespace
          redeclarations, run the positive case on i686/AMD64, and reject a
          redeclaration whose call argument expression differs as an ambiguous
          overload on both targets; GCC C++20 agrees.
    - [x] Compare structural identity for requires-expression subscript,
          assignment, comma, unary `noexcept`, `typeid` and string-literal
          simple requirements while mapping renamed local parameters by
          position. A dedicated target is part of `CXX_REGRESSION_TARGETS`; the
          positive friend call executes after i686/AMD64 generation, while a
          changed subscript expression is rejected with one exact ambiguity
          diagnostic and no `.ro` on either target. GCC C++20 agrees.
    - [ ] Complete constraint equivalence for remaining expression forms,
          concept normalization, and the full standard constraint model;
          template-template defaults/signatures
          beyond the one-type-parameter form, non-type defaults outside the
          supported integral constant-expression evaluator, general overload/redeclaration
          matching beyond the tested type/integral-non-type and identical-pack
          forms, and the remaining standard friend cases.
    - [x] Merge matching friend function-template redeclarations whose
          template parameter list contains the same type pack and whose
          function parameter pack shapes match. Accumulate friendship granted
          by two classes, then merge the namespace definition; execute calls
          with multiple pack arguments for both granting classes. The
          i686/AMD64 friend-function gate passes with x64 host execution, and
          GCC C++20 compiles and executes the same fixture.
    - [x] Search the exact associated namespace and its transitive inline
          namespace set for ADL declarations. Normalize inline-namespace
          entities to their innermost enclosing non-inline namespace; do not
          incorrectly search unrelated enclosing non-inline namespaces or
          follow using-directives. Test nested-namespace and both inline-set
          directions, plus an outer-namespace rejection. The i686/AMD64
          friend-function gate and GCC C++20 comparison pass.
    - [x] Keep ordinary in-class non-template friend functions hidden from
          global ordinary and qualified lookup, admitting them only through
          ADL when an argument is the granting class or a derived class.
          Preserve visibility when an ordinary namespace declaration precedes
          the friend declaration.
          The i686/AMD64 friend-function gate verifies accepted owner-class
          calls, unrelated unqualified and qualified rejection, later
          namespace-definition visibility, and GCC C++20 comparison.
    - [x] Implement defined, nonvirtual, non-overloaded member-function
          pointers for unqualified, `const`, `volatile`, and `const volatile`
          methods: preserve the owner and method cv in the type, form its code
          address, and invoke it through `.*` and `->*` with an ABI-level
          implicit object argument. Verify Itanium names against GCC, including
          repeated owner-type substitutions and rejection of dropped cv. The
          focused `test-cxx-member-pointer-functions` test generates i686/AMD64
          code and executes the AMD64 output; the full `test-cxx` suite passes.
    - [x] Extend member-function pointers to target-typed overload resolution,
          inherited lookup/access and owner conversions, ref/noexcept-qualified
          methods, and virtual dispatch. Both native backends and typed SSA use
          the Itanium `<ptr, adj>` representation, including direct and virtual
          method encoding, null-preserving adjustment, ABI arguments/returns,
          receiver adjustment, virtual-base object lookup, and virtual dispatch.
          `test-cxx-member-pointer-functions` covers size, null/equality,
          parameter/return passing, multiple-inheritance owner conversion and
          round-trip equality, ref/noexcept conversions, friend access, private
          and ambiguous lookup, invalid repeated/virtual owner paths, and
          virtual dispatch. i686/AMD64 object generation passes; generated x64
          execution and GCC C++20 name comparison pass. The typed-SSA fixture
          emits 12 functions with zero fallback notices at `-O0` and `-O2` on
          both targets. No placeholder lowering is used.
    - [x] Resolve a unique supported nonvirtual member-function address inside
          an inline member body after the class methods are registered; form
          the same pointer type as an out-of-class `&Class::method` and execute
          it through `.*` on i686/AMD64 generation and x64 host execution;
          compare the fixture with GCC C++20. `test-cxx-member-pointer-functions`
          passes.
    - [x] Resolve a later-declared, in-class-defined static member-function
          address inside an inline member body as an ordinary function pointer,
          then call it without an implicit object argument. The
          `test-cxx-member-pointer-functions` gate passes i686/AMD64 generation,
          generated x64 host execution, and GCC C++20.
    - [x] Resolve a unique inherited nonvirtual, non-overloaded method in
          `&Derived::method` using the registered base method, preserve the
          declaring base as the pointer owner, and apply inherited access
          rules. Verify the Itanium function-signature name against GCC,
          execute the derived-object call on the x64 host, and reject forming
          a pointer through a private base on i686/AMD64.
    - [x] Resolve a target-typed overloaded member-function address by exact
          return/parameter/cv signature and owner match in declarations,
          assignments, and function arguments. Compare the Itanium wrapper
          name with GCC, execute non-const/const and parameter-overload cases
          on x64, and reject missing-target/no-match cases on i686/AMD64.
    - [x] Lower calls through supported nonvirtual member-function pointers
          into typed SSA by converting their pointer-width code-address value
          to the backend's indirect-call pointer type. Test dot/arrow and all
          supported method cv forms on i686/AMD64 at `-O0` and `-O2`; the
          expanded gate below checks the exact function count and zero fallback
          notices on each target.
    - [x] Include overloaded member-function address selection, local
          pointer initialization, typed function-argument and return contexts,
          indirect calls, and null round trips in the verified-SSA regression.
          Require twelve emitted functions and zero fallback notices on
          i686/AMD64 at `-O0` and `-O2`.
  - [x] Preserve member-pointee `const` through same-owner and combined
        derived-owner conversions, reject qualification removal, and require an
        lvalue for built-in scalar assignment through an xvalue-selected member.
        Keep valid class xvalue copy assignment working; a host regression caught
        address assignment in this path, now fixed in both native backends.
        Verify positive and negative cases on i686/AMD64 with
        `test-cxx-member-pointer-data` and compare the positive source against
        GCC C++20.
  - [x] Preserve xvalue category for a non-reference data member selected
        through an xvalue object; sema, `decltype(auto)`, and both i686/AMD64
        codegens agree. Reference data members remain lvalues. Cover reference
        returns and execution in `test-cxx-function-template-references`.
  - [x] Lower reference data-member access through its stored referent address
        and use the referred-to type for reads and scalar writes. A templated
        reference-member constructor/read/write regression passes i686/AMD64
        generation and execution at default optimization and `-O2` in
        `test-cxx-class-template-methods`.
  - [x] Preserve a named class xvalue through explicit `static_cast<T&&>` and
        cv-qualified virtual-base reference binding; apply the vbtable adjustment
        exactly once through local initialization and reference-parameter calls.
        Verify both target codegens and x64 execution, including diamond layout
        and destructor ordering, in `test-cxx-function-template-references`.
  - [x] Preserve the complete derived temporary when an explicit
        `static_cast<Base&&>` binds to a public non-virtual or virtual base;
        apply the base adjustment after selecting the lifetime-extended stack
        slot, and verify destructor order plus virtual-diamond member access
        with i686/AMD64 generation and x64 execution in
        `test-cxx-function-template-references`.
  - [x] Deduce local `auto&&` bindings from both lvalue and xvalue initializers,
        preserve aliasing through reference collapsing, and verify i686/AMD64
        code generation plus x64 execution in `test-cxx-function-template-references`.
  - [x] Lower scalar local reference aliases as pointer-backed locals in typed
        SSA, preserving read/write aliasing for `T&&`; assert zero fallback on
        i686/AMD64 and execute the mutation test on the host in
        `test-verified-backend`.
  - [x] Materialize scalar prvalues bound to local references in stable
        function-frame storage, including lifetime extension for `const T&`;
        verify the value survives a subsequent call on i686/AMD64 codegen and
        x64 execution in `test-cxx-function-template-references`.
  - [x] Extend nontrivial aggregate prvalue lifetime for both `const T&` and
        `T const&` local bindings, preserve the temporary across later calls,
        and run destructors once in reverse declaration order at scope exit;
        verify i686/AMD64 generation and x64 execution in
        `test-cxx-function-template-references`.
  - [x] Preserve a complete derived prvalue when a local reference binds to
        its non-virtual or virtual base; initialize virtual-base tables before
        adjusting the reference, and run derived, direct-base, and virtual-base
        destructors exactly once in reverse lifetime order, including inherited
        implicit cleanup. Verify i686/AMD64 generation and x64 execution in
        `test-cxx-function-template-references`; the complete `test-cxx` gate
        and related virtual-base, array-destructor, member-lifetime, and global-
        constructor gates pass.
  - [x] For non-virtual, `noexcept` destructor trees without class-array or
        custom cleanup subobjects, explicitly invoked user destructor bodies
        destroy members and direct bases in reverse construction order after
        the body. Complete scope-exit destruction runs exactly once. Verify
        i686/AMD64 emission with zero fallback and x64 direct-call/scope-exit
        execution in `test-verified-cxx-destructor` and
        `test-cxx-destructor-body`; the complete `test-verified-backend` gate
        passes.
  - [x] Extend explicit-destructor subobject cleanup to fixed-size class-array
        members, including multidimensional arrays. Lower one reverse-order
        runtime walk over every element in legacy x86/x64 and typed SSA; verify
        i686/AMD64 emission with zero fallback and x64 execution of
        body -> `[1][1]` through `[0][0]` -> base order (`943211`). The legacy
        explicit-call and automatic-scope tests also pass. The complete
        `test-cxx`, `test-verified-backend`, `test-optimize`, and
        `test-debug-info` gates pass.
  - [x] Extend explicit-destructor subobject cleanup through validated scalar
        cleanup-hook wrapper members. The enclosing destructor body runs first,
        then the validated member cleanup runs exactly once; verify automatic
        scope cleanup in the legacy backend and explicit destruction in typed
        SSA, with order `94`, i686/AMD64 emission, and zero fallback in
        `test-cxx-destructor-body` and
        `test-verified-cxx-destructor`. The full `test-cxx` and
        `test-verified-backend` gates pass.
  - [x] Extend explicit-destructor subobject cleanup through complete-object
        virtual bases for validated `noexcept` trees. Save the evaluated
        receiver before the destructor body, then destroy direct bases and
        shared virtual bases exactly once in reverse order. Verify order `9321`
        and receiver mutation safety in legacy x64 execution, i686/AMD64 code
        generation, and typed-SSA zero-fallback emission in
        `test-cxx-destructor-body` and `test-verified-cxx-destructor`.
  - [x] Preserve explicit-destructor subobject cleanup when the explicitly
        invoked root destructor throws but every remaining base/member
        cleanup is `noexcept`. Register callbacks with the runtime's current
        exception frame across the call boundary; preserve the saved receiver
        and run reverse-order cleanup exactly once. Verify catch value `47`,
        receiver mutation, order `9321`, i686/AMD64 emission, and x64 runtime
        in `test-cxx-exception-cleanup`.
  - [x] Lower that throwing-root cleanup in typed SSA by registering the
        `noexcept` base/member callbacks with the dynamic current exception
        frame before the root call, then unregistering before normal reverse-
        order destruction. Verify both runtime symbols are present, with zero
        fallback for i686/AMD64 in
        `test-verified-cxx-explicit-throwing-destructor`.
  - [x] Extend throwing cleanup to subobject destructors that may themselves
        throw. Sema now carries the throwing-destructor allowance through
        nested bases and members; typed SSA registers their callbacks and
        drains them in reverse order. The dedicated
        `test-verified-cxx-explicit-throwing-destructor` covers a potentially
        throwing base and member with zero fallback on i686/AMD64. Runtime
        cleanup now preserves unconsumed records
        during normal scope exit, catches a cleanup exception in the frame being
        left, and terminates if a callback throws while another exception is
        unwinding. `test-cxx-exception-cleanup` verifies partial progress/order
        `921` and catch value `53`, normal-leave catch value `61`, and isolated
        double-throw termination. The production `rin_runtime.c` builds into
        signed runtime RLLs for both architectures in
        `test-rinvalidate-dynamic-tls`; bootstrap source syntax checks pass for
        both target widths. The broader cleanup fixture pins its one unrelated
        `dependent_dmi` fallback explicitly.
  - [x] Resolve data-member access exposed through a virtual base using the
        active subobject's vbtable and member-in-base offset; verify inherited
        access from both arms of a shared virtual diamond during destruction,
        direct and pointer-member access through both arms, and exactly-once
        virtual-base cleanup, on i686/AMD64 codegen and x64 execution in
        `test-cxx-function-template-references`.
  - [x] Represent object destructor cleanup as nested cleanup plans so large
        automatic arrays use reverse-order runtime loops instead of a fixed
        per-element expansion; connect normal scope exits, exception callback
        registration/unregistration, global finalizers, and optimizer analyses.
        Verify with a 4101-element local array plus existing array/member/
        reference lifetime gates on both target codegens and x64 execution.
  - [x] Rebind constructor parameters and `this` through nested scalar member-
        initializer expressions, including member calls. The move-constructor
        regression verifies i686/AMD64 objects and x64 execution in
        `test-cxx-inline-aggregates`.
  - [x] Complete constructor-expression rebinding for every current
        expression-bearing AST form in both native backends. Generic/requires
        expressions, lambda captures, pack-expansion patterns, temporary
        cleanup plans, ownership-move/close metadata, and temporary-result
        aliases are recursively rebound; constructor identifiers match their
        resolved parameter declaration so nested scopes cannot capture them by
        name. Unknown future expression kinds emit a diagnostic instead of
        lowering with stale constructor references. The focused build and
        runtime regression pass in `test-cxx-inline-aggregates`; the complete
        `test-cxx` suite also passes after this follow-up. The i686 and AMD64
        objects are generated, i686 cleanup symbols/sections are inspected,
        and the x64 host executes the observed custom-move regression. The
        aggregate `test-ci` remains incomplete here because its WSL bootstrap
        command is denied by the host (`E_ACCESSDENIED`); do not treat this as
        a complete production-gate result.
  - [x] Verify reverse-order runtime finalization for large defined
        namespace-scope arrays using 4101-element explicit-empty and
        no-initializer arrays; verify reverse element and declaration order on
        both target codegens and the x64 host in `test-global-finalizers`.
        Function-local static destructors and TLS forms requiring unsupported
        generated default-constructor or dynamic-aggregate lowering remain open.
        Supported forms now use per-object guards and first-use cleanup
        registration; exception retry/order and runtime acceptance remain open.
  - [x] Preserve C++ conditional-expression lvalue/xvalue category and exact
        cv-qualified result type when both operands match; lower the selected
        object address on i686/AMD64 and test reference returns, `decltype(auto)`,
        and narrow-character assignment in `test-cxx-function-template-references`.
  - [x] Preserve C++ comma-expression lvalue/xvalue category in reference
        returns while evaluating the left operand; verify aliasing and its side
        effect on i686/AMD64 code generation and x64 execution in
        `test-cxx-function-template-references`.
  - [x] Read bit-field metadata only from member-expression AST nodes in both
        assignment backends; conditional expressions share union storage with
        those fields and must not be misidentified as bit-fields.
- [x] bounded Itanium ABI mangling、exceptions、RTTI、static initialization
  - [x] Implement the validated C++ empty-base optimization for a leading,
        non-polymorphic direct empty base, preserve the standard same-type
        base/member non-overlap rule, and cover i686/AMD64 object generation.
  - [x] Implement bounded C++20 `[[no_unique_address]]` layout for empty,
        trivial non-static data members, retain ordinary storage for unsupported
        member types, preserve same-type base/member non-overlap, propagate the
        attribute through class templates, and diagnose pre-C++20 or invalid
        placements on i686/AMD64.
  - [x] `typeid(T)`と非多相式の静的typeinfo identityをi686/AMD64で生成し、
        同一型のidentity共有・異なる型の分離を実行回帰。
  - [x] static typeinfo identity hashで参照型と最上位cv修飾を正規化し、
        入れ子のcv修飾・関数pointer引数signature・pointer-to-memberのowner
        classを区別し、32段を超えるpointer型も末端まで識別する。
        const/non-const pointee、function signature、member owner、reference、
        deep pointerのidentityを実行テストし、両archの`.ro`/`.rin`を検証。
  - [x] Give static typeinfo symbols collision-safe, cross-translation-unit
        identities for every type form currently represented by the frontend;
        the supported-type regressions and golden outputs pass. Remaining
        local-class template-scope coverage is tracked in the unchecked item
        below.
    - [x] Replace the non-class 64-bit structural hash with a length-prefixed
          canonical encoding for represented structural types, including
          distinct plain `char`, `signed char`, and `char8_t` identities.
    - [x] Derive supported class-template identity from its source template,
          namespace path, and complete arguments instead of internal
          `.__instanceN` names; include namespace-qualified type arguments.
    - [x] Run the dual-architecture `typeid` regression, refresh deterministic
          golden outputs, and add translation-unit-order coverage for class
          template identity. Two translation units instantiate the same class
          template in different orders, compare `typeid` identity, link both
          `.ro` files for i686/AMD64, and validate the resulting `.rin` images.
    - [x] Resolve namespace-qualified enum type-ids and distinguish same-named
          enums from separate namespaces in `typeid` identity.
    - [x] Give block-scope and unnamed enum tags distinct parser identities,
          restore bindings across nested blocks, and emit local typeinfo with
          translation-unit-local linkage; verify repeated, sibling, and nested
          enum identity with host execution and both target widths.
    - [x] Include the translation-unit path in anonymous-namespace class and
          enum typeinfo identities; compare same-named types with global types.
    - [x] Give function-local C++ classes per-scope identities and
          translation-unit-local typeinfo linkage; test repeat use, nested
          shadowing, same-name functions and cross-translation-unit distinction.
    - [x] Lower ordinary local-class member functions and basic explicit
          constructors with scope-distinct mangling and local symbol linkage;
          execute sibling and cross-translation-unit cases and validate both
          target-width RIN images.
    - [ ] Complete local classes inside function-template instantiations,
          including dependent bases, parameter packs, and the complete
          constructor/member ABI.
      - [x] Bind each local class to the enclosing template arguments, clone
            dependent field layouts and member-function bodies per
            specialization, and preserve distinct local `typeid` identities;
            execute distinct `int`/`long long` specializations and validate
            i686/AMD64 objects.
      - [x] Associate method-local classes with their owning local-class
            specialization; execute nested member-function cases for `int`
            and `long long` on the host and validate i686/AMD64 objects.
      - [x] Discard unselected `if constexpr` local-class definitions during
            template substitution; execute the selected specialization and
            verify the discarded method is absent from generated assembly.
      - [x] Expand dependent base packs for zero, one, and two concrete bases;
            verify target-width layouts and host execution.
      - [x] Substitute dependent base constructor initializers and expand
            `Bases()...`; verify constructor side effects and declaration order
            for zero, one, and two pack elements.
      - [x] Preserve expression-shaped direct initialization of a local class
            in a function-template pattern until specialization resolves its
            incomplete class layout and constructor set.
      - [x] Substitute dependent `using Base<T>::member` and
            `using Base<T>::Base` declarations; execute inherited-constructor
            cases for `int` and `long long` and validate both target objects.
      - [x] Defer dependent-base `this->member` field and method lookup until
            local-class specialization; execute `int` and `long long` cases and
            validate both target widths plus x64 host behavior.
      Dependent base type patterns now remain attached to the local class until
      specialization, where type-pack bases are expanded before layout and
      virtual validation. Local member signatures/bodies and pack-expanded
      constructor base initializers are substituted against those elements;
      templated base initializer names retain their type pattern so overloads
      bind to the matching concrete base specialization. Specialized methods
      also preserve declaration attributes such as `constexpr`, `consteval`,
      `nodiscard`, deprecation, and their resolved owning class type, along
      with the function type's variadic/prototype metadata.
      Dependent `using Base<T>::member` declarations retain and substitute
      their base type, and public direct `using Base<T>::Base` resolves the
      inherited constructor set against the exact specialized base. The
      focused local-class and inherited-constructor targets pass for the
      dependent-base/layout/constructor forms checked above. The complete
      native-Windows `test-cxx` aggregate also passes after updating the stale
      array-member diagnostic expectation and covering class-typed empty-brace
      default member initialization through an inherited constructor, plus
      dependent-base field and method lookup after specialization. Keep this
      parent unchecked until the broader constructor/member ABI and dependent
      member lookup forms are verified.
      Synthesized inherited constructors retain source constexpr/consteval,
      nodiscard/deprecation, noreturn/inline, and function-type prototype metadata;
      variadic and prototype-less source constructors are rejected. The parser
      translation unit passes `gcc -Wall -Wextra -std=c11 -fsyntax-only`, and
      the full native-Windows `test-cxx` aggregate passes on the current checkout,
      including class-typed default-member initialization through an inherited
      constructor.
      - [x] Cover fixed-size class-array members initialized through inherited
            constructors and ordinary constructor prologues, including nested
            arrays, empty-brace array default member initializers, and cleanup
            of inherited elements. Assert row-major construction values and
            reverse per-element destruction. `make SHELL=cmd.exe
            test-cxx-inherited-constructor` passes freestanding i686 execution,
            AMD64 host execution, and object generation for both target widths.
      - [x] Lower implicit default-initialization of supported fixed-size class
            array members in verified IR without zero-initializing their
            elements; register each element's destructor after its lifetime
            begins and verify reverse destruction for i686/AMD64 objects with
            zero fallback in `test-verified-cxx-destructor`.
  - [x] Parse and instantiate a dependent direct base in an ordinary class
        template, defer its base layout and default-member initialization,
        resolve `using Base<T>::Base`/`using Base<T>::member`, and lower
        `this->` field and method lookup. The `int`/`long long` regression
        emits both target widths and executes on x64 in
        `test-cxx-class-template-dependent-base`.
  - [x] Complete ordinary class-template dependent inheritance coverage for
        base packs, dependent virtual bases/overrides, access and ambiguity
        rules, remaining dependent default-member initializer forms, and
        transitive dependent member lookup. The complete focused target passes
        freestanding i686 execution, x64 host execution, diagnostics, and both
        target object generations.
    - [x] Run the ordinary dependent-base regression with signed and unsigned
          64-bit scalar default member initializers on i686 and AMD64, checking
          high-word preservation and narrow-to-wide sign/zero extension.
          `make SHELL=cmd.exe test-cxx-class-template-dependent-base` passes
          freestanding i686 execution, x64 host execution, and both target object
          generations.
    - [x] Expand ordinary class-template dependent base packs and matching
          `Bases()...` constructor initializers for zero, one, and two bases;
          verify construction order on freestanding i686 and x64 host execution
          and emit both target objects with
          `make SHELL=cmd.exe test-cxx-class-template-dependent-base`.
    - [x] Instantiate a dependent virtual direct base, initialize it from the
          most-derived constructor, and access its dependent member for `int`
          and `long long`; verify freestanding i686 and x64 host execution plus
          both target objects with
          `make SHELL=cmd.exe test-cxx-class-template-dependent-base`.
    - [x] Verify a dependent virtual override dispatches through a base pointer
          for `int` and `long long`, with freestanding i686 and x64 host
          execution plus both target objects using the same focused target.
    - [x] Support bounded explicit scalar-argument initialization of a direct
          dependent polymorphic virtual base. The regression verifies override
          dispatch through the base pointer for `int` and `long long`; the
          focused target passes freestanding i686 and x64 host execution plus
          both target object generations.
    - [x] Apply private/protected base access to inherited fields and methods;
          exercise access from the derived class and reject access from outside.
    - [x] Diagnose repeated dependent-base data names and member-function lookup
          ambiguity before overload ranking; verify private/protected access
          diagnostics on i686 and AMD64.
    - [x] Initialize dependent class-valued fields from bounded brace DMIs and
          dependent scalar fields from constant arithmetic DMIs for int and
          long long on both target widths.
    - [x] Diagnose mixed inherited data-member/function name ambiguity for both
          field access and function-call lookup on i686 and AMD64; verify a
          same-named direct derived field hides the dependent base field through
          runtime execution in `test-cxx-class-template-dependent-base`.
    - [x] Verify dependent `using Base<T>::member` selection when multiple
          direct bases provide the same field or function name. The positive
          fixture selects the second base; the complete focused target passes
          freestanding i686 execution, x64 host execution, and both target
          object generations, including the separate parameter-DMI regression.
    - [x] Cover further dependent name-lookup paths beyond mixed-name
          ambiguity, direct-member hiding, and verified using-selection:
          inherited fields through two dependent nonvirtual bases with
          nonzero offsets, inherited overload selection, and a shared virtual
          base diamond for int and long long; diagnose repeated transitive
          nonvirtual-base ambiguity for fields and methods. The dedicated
          positive/negative targets and complete parent target pass on i686
          and AMD64.
    - [x] Resolve dependent nested aliases through both `Base<T>::value_type`
          and `T::value_type`; verify return types, local declarations, and
          runtime values for i686 and AMD64 with
          `make SHELL=cmd.exe test-cxx-class-template-dependent-nested-type`.
    - [x] Resolve chained dependent type-ids such as
          `typename T::value_type::nested_type` after template substitution in
          class-template members and function-template return types. Apply
          access checks at each alias hop and diagnose missing/private later
          components; verify i686 freestanding execution, AMD64 execution,
          exact negative diagnostic counts for both targets, and GCC C++20
          acceptance/rejection in `test-cxx-class-template-dependent-nested-type`.
    - [x] Preserve the complete nested path in requires-expression type
          requirements, including `T::value_type::nested_type` and a dependent
          class-template-id owner. Resolve the path after substitution, make a
          missing later alias evaluate false, and verify i686/AMD64 generation,
          AMD64 host execution, and GCC C++20 acceptance in
          `test-cxx-requires-type`.
    - [x] At concrete class-template substitution, diagnose missing and
          inaccessible dependent nested aliases instead of retaining an
          unresolved layout type; keep still-dependent names deferred and
          resolve aliases inherited from a direct public base. Diagnose
          conflicting aliases from multiple direct public bases. Gate the
          diagnostics and inherited-alias positive case on i686 and AMD64.
    - [x] Generalize inherited dependent nested-type lookup across multiple
          base levels, shared virtual base subobjects, and protected-access
          context; keep this separate from direct-base alias substitution.
          Positive coverage includes non-template and class-template chains,
          shared virtual and shared virtual template bases, protected access
          from a derived template, and a function-template return type. Negative
          coverage checks ambiguous inherited aliases and unrelated protected
          access on i686/AMD64. The focused target passes i686 freestanding and
          AMD64 host execution plus PE-i386/object generation; the broader
          nested-type target and the requires/function-template fallback tests
          pass too. Adjacent template-closing `>>` and a normal right-shift
          expression are both covered.
    - [x] Treat missing or inaccessible dependent nested aliases in concrete
          function-template return/parameter signatures as substitution failure;
          preserve still-dependent names and requires-expression substitution,
          and continue overload resolution to a viable fallback. Verify precise
          diagnostics and fallback compilation on i686/AMD64 with
          `test-cxx-function-template-dependent-nested-type` and
          `test-cxx-requires-type`.
    - [x] Evaluate bounded scalar DMIs that read an earlier direct scalar field
          through either `this->field` or unqualified member lookup in a class
          with a dependent base. Verify `int` and `long long` on freestanding
          i686, x64 host execution, and both target object generations.
    - [x] Lower a dependent scalar DMI that reads an earlier member initialized
          from a constructor parameter, through both `this->field` and
          unqualified lookup. Verify `int` and `long long` by freestanding
          i686 execution, x64 host execution, and both target object generations
          in `test-cxx-class-template-dependent-base`.
    - [x] Lower bounded scalar-return calls, comma expressions with global
          side effects, one-dimensional fixed scalar-array DMIs, and
          destructor-bearing class-valued DMIs. Verify per-object effects and
          member cleanup by i686 freestanding execution, x64 host execution,
          and object generation for both targets with
          `make SHELL=cmd.exe test-cxx-class-template-dependent-nested-type`.
    - [x] Parse and lower nested fixed-size scalar-array DMIs, preserving
          source dimension order; verify all values in `int` and `long long`
          instances with freestanding i686 execution, x64 host execution, and
          both target object generations in
          `test-cxx-class-template-dependent-nested-type`.
    - [x] Lower `this->member` as an argument to a scalar-returning call in a
          dependent DMI; verify `int` and `long long` with freestanding i686
          execution, x64 host execution, and both target objects in
          `test-cxx-dependent-dmi-call-argument`. Verify partial construction
          cleanup when a later DMI constructor throws: the completed member's
          destructor runs and the throwing member's destructor does not. The
          x64 host exception-runtime regression also checks that a completed
          DMI member is destroyed once when a later statement throws; both
          target objects are generated by `test-cxx-exception-cleanup`.
  - [x] 多相classのglvalue `typeid(expr)`をvtableのmost-derived typeinfoへ
        lowerし、null polymorphic pointerをRinOS `bad_typeid` exceptionへ
        transferする。non-glvalueの多相式は明示診断し、i686/AMD64の
        non-null identity、null catch、`.ro`、unsigned-v3 `.rin`、
        `rinvalidate`を回帰検証する
  - [x] bounded `type_info::hash_code()`をtypeinfo identity objectからの
        pointer-sized hash loadへlowerし、同一型一致・異型分離・引数付き
        呼出しの明示診断をi686/AMD64で回帰検証する。`type_info`同士の
        `==`/`!=`はidentity address比較へlowerし、relational比較は診断する
  - [x] bounded `type_info::name()`をtypeinfo identity object内の安定した
        NUL終端文字列ポインタへlowerし、静的・多相dynamic `typeid`の
        非空name、引数付き呼出し診断、両archの`.ro`／unsigned-v3／
        `rinvalidate`回帰を追加する
  - [x] bounded `type_info::before()`をtypeinfo identity addressの
        実装定義順序比較へlowerし、同一型false・異型の相互排他を
        i686/AMD64の実行回帰で検証する
  - [x] static/non-polymorphic `typeid` と `type_info` の
        `==`/`!=`、`hash_code()`、`name()`、`before()`を verified
        typed-SSAへlowerし、identity metadata・pointer-width field load・
        i686/AMD64 `.ro` objectを検証する。dynamic polymorphic `typeid`、
        complete `type_info` API、aggregate/exception SSAは引き続き未完了
- [x] cross-library exception transport and cleanup across `.rll` boundaries
- [ ] remaining full Itanium ABI、`type_info` API、complete static/TLS
      destructor semantics
- [ ] static/TLS destructor and exception cleanup interaction. Both native
      backends use guarded first-use initialization for supported
      function-local static and destructible TLS objects, connect guard-abort
      callbacks against the dynamically active exception frame, and register
      cleanup through `__cxa_atexit`/`__cxa_thread_atexit`. Constructor-bearing
      function-local TLS now takes the dynamic initialization path instead of
      being mistaken for zero initialization.
  - [x] Throw from a function-local class-static constructor on its first
        attempt, verify current-frame guard abort and successful retry, then
        verify DSO-matched reverse `__cxa_atexit` destruction and idempotent
        finalization under the i686/AMD64 POSIX host exception harness.
  - [x] Apply the same throw/retry/current-frame cleanup checks to a
        function-local `thread_local` class, verify its constructor runs,
        `__cxa_thread_atexit` receives registrations, and three TLS destructors
        run once in reverse order on the i686/AMD64 POSIX host harness.
  - [x] Apply throw/retry and per-thread destructor registration to a
        namespace-scope `thread_local` class with a dynamic constructor; verify
        its first-use guard aborts on throw, its next access initializes once,
        and its destructor joins reverse-order exactly-once TLS finalization on
        the i686/AMD64 POSIX host harness.
  - [x] Emit local-exec TLS data and relocation records from `-S`, assemble
        both target variants, and verify `R_386_TLS_LE` / `R_X86_64_TPOFF32`
        records with the host object inspector before executing the generated
        retry/finalization cases.
  - [x] Correct i686 cdecl argument order for function-local static
        `__cxa_atexit`; verified generated callback/object/DSO values and
        successful destructor execution.
  - [ ] Run the same regression against the shipped RinOS `rincrt` on target
        systems; host harness coverage does not replace hardware/runtime
        acceptance.

## 4. IR / optimization

- [x] scalar typed SSA IRとCFGの検証済みsubset
  - [x] scalar/pointer SSA value、basic block、phi、terminator、dominance/use-def/type verifier
  - [x] scalar ASTのalloca/load/store、scaled pointer GEP、短絡条件・論理式SSA loweringとif/while/do/for/switch CFG verification
  - [x] non-escaping entry scalar allocaのdominance-frontier mem2regとphi挿入
- [x] 定数条件分岐のSSA branch化、到達不能blockと不要phi入力の除去
- [x] C goto/labelを事前収集したSSA CFG blockへlowerし、外側に1つのruntime
      boundを持つVLA（scalar/pointer element、および固定bound array element）の
      scope退出でsaved stack pointerを復元する。normal scope exitと
      `for`/`while`/`do`のbreak/continueも検証済み。C++では検証済みnoexcept
      cleanupを伴うscope退出gotoも対応済み。追加runtime-bound次元などのVLA形式と
      ここで未実装のthrowing/partial-construction cleanup形式は引き続きsubset外。
- [ ] aggregate/vector/exceptionを含む全frontendのtyped SSA lowering
  - [x] Lower C23 variably modified `typeof` expressions, VLA typedef-bound
        capture, cast type-name bounds, and dynamic `sizeof(VLA)` into typed
        SSA. `test-c23-typeof-type-name` requires five verified functions and
        zero fallback on i686/AMD64, then executes both objects; the fixture
        covers single-evaluation and shared typedef bounds.
  - [x] `switch`の`case`ラベルを`while`/`do`/`for`本体内から収集し、通常のswitch
        dispatchとloop backedgeを保ったverified SSA CFGへlowerする。i686/AMD64
        `-O2` emissionとx86_64通常／最適化後実行を回帰検証
  - [x] `switch`内のgoto label配下にある`case`を収集し、前段caseからのgotoと
        switch直dispatchを同じlabel/case CFGへ合流させる。両archの通常/O2で
        fallbackなし、x86_64で両経路を実行検証
  - [x] 偽になる条件式の内側にあるnested `case`へswitch dispatchする経路を、
        AMD64 verified backendの`-O0`/`-O2`でfallbackなし生成し、case/defaultの
        runtime結果を検証
  - [x] `do`/`for`本体の`return`/`break`終端をSSA CFGとして保持し、continue edgeが
        必要とするcondition/increment blockも検証する。switch内caseのreturn/break/
        continueを両arch通常/O2でfallbackなし、x86_64実行で検証
  - [x] i686 cdeclの64-bit整数を、EDX:EAXのverified SSA return-pair、8-byte引数、
        local/global load、narrow cast、加減算のcarry/borrow、bitwise、
        signed/unsigned比較、0..63-bit shift、direct／間接function-pointer call、
        div/modへ接続し、
        実行回帰で検証
  - [x] i686 wide-scalarを検証済みのlow/high i32 SSA pairとして扱い、pair
        invariant、符号／ゼロ拡張、narrow integer／pointer cast、64-bit shift
        count、演算、conditional phi、cdecl引数とEDX:EAX戻り値を一貫して接続。
        scalar i64を32-bit MIRへ漏らす経路は明示的に拒否する
  - [x] Keep aggregate reference casts and reference-returning calls as object
        addresses in SSA; verify a user-defined `T&&` conversion return has no
        fallback on i686/AMD64 and execute the generated x64 `.ro` alias test
        in `test-verified-cxx-reference-return`.
  - [x] Inline a non-virtual direct base constructor in typed SSA when it is
        `noexcept`, has an empty body, and initializes scalar base fields from
        forwarded scalar parameters. Verify the derived conversion-return path
        has zero fallback on i686/AMD64 and execute its x64 `.ro` result check
        in `test-verified-cxx-reference-local`.
  - [x] Lower supported trivial-aggregate conditional prvalues into one
        branch-selected object slot and pass that slot to a reference parameter;
        verify i686 sret and x64 register-return paths emit without fallback,
        then execute the generated x64 `.ro` in
        `test-verified-cxx-conditional-aggregate`.
  - [x] Lower non-throwing class-prvalue comma expressions bound to reference
        parameters through typed SSA, execute fixed-member destructor plans
        after the containing call in reverse construction order, and verify
        nested parent/member cleanup, single and paired temporaries, exactly-once
        cleanup/comma side effects, fallback-free i686/AMD64 objects, and x64
        execution in `test-verified-cxx-temporary-cleanup`. Fixed-size array
        loops in call-temporary cleanup plans are covered separately.
  - [x] Register flat `noexcept` cleanup callbacks for a fully constructed
        class-prvalue reference argument before a potentially throwing call;
        on normal return, unregister before inline cleanup, and on throw let the
        active exception frame run the cleanup before the handler. Verify the
        regular x64 throw/normal paths (results `472`/`22`) and zero typed-SSA
        fallback for i686/AMD64 at `-O0`/`-O2`, including runtime registration
        imports, in `test-verified-cxx-throwing-temporary`.
  - [x] Lower same-type noexcept class-prvalue conditional arguments bound to
        reference parameters. Pass the final lifetime-owner storage directly
        to the selected branch's sret call or capture a one-/two-register
        aggregate return directly into that storage; run the validated
        fixed-member destructor plan once after the outer call. Verify both
        branch choices, cleanup order, fallback-free i686/AMD64 objects, and x64
        execution in `test-verified-cxx-temporary-cleanup`. A non-virtual,
        `noexcept` ordinary member-function call returning the same class is
        also covered as a branch arm. Field-backed inline member calls,
        conversions, potentially throwing expressions, and automatic-object
        cleanup remain outside this conditional-temporary subset.
  - [x] Validate noexcept fixed-member cleanup plans recursively for class
        prvalue reference arguments of a selected conditional branch call;
        emit the inner temporary cleanup immediately after that call and the
        conditional result cleanup after its consumer. Verify both branch
        choices, inner/outer cleanup order, fallback-free i686/AMD64 objects,
        and x64 execution in `test-verified-cxx-temporary-cleanup`.
  - [x] Recursively lower nested same-type class-prvalue conditionals when
        their leaves are supported noexcept calls. Verify both nested branch
        choices, exactly-once destruction of the selected temporary,
        fallback-free i686/AMD64 objects, and x64 execution in
        `test-verified-cxx-temporary-cleanup`.
  - [x] Lower fixed-size array-loop cleanup plans recursively for nontrivial
        subobjects of class-prvalue reference temporaries, including nested
        arrays. Emit reverse element destruction and verify the observed order
        in both typed-SSA and regular codegen paths; typed SSA passes `-O0` and
        `-O2` on i686/AMD64 with no fallback in
        `test-verified-cxx-temporary-cleanup`.
  - [x] Lower validated `noexcept` fixed-size array cleanup plans for automatic
        struct/array objects with supported aggregate initialization. Emit
        destruction on lexical-block exit, early return, loop `break` /
        `continue`, and switch `break`, including nested arrays; allocate
        synthetic loop indices before control-flow splits so each cleanup path
        has a dominating slot. Verify reverse destruction on normal/early
        exits, exactly-once loop exits, and both switch cases, with no typed-SSA
        fallback on i686/AMD64 at `-O0` and `-O2` in
        `test-verified-cxx-automatic-array-cleanup`, included by
        `test-verified-backend` and the production CI gate.
  - [x] Register completed reference-bound argument temporaries before
        evaluating a later potentially throwing sibling argument, then verify
        exactly-once cleanup and reverse construction order with one and two
        live temporaries. The `noexcept`-callee regression runs through typed
        SSA with no fallback on i686/AMD64 at `-O0` and `-O2` in
        `test-verified-cxx-throwing-temporary`.
  - [ ] Throwing initializer evaluation before a cleanup owner is fully
        constructed, additional constructor-driven partial construction/unwind
        shapes for automatic reference-bound prvalues,
        potentially throwing cleanup callbacks, VLA forms outside the currently
        verified subset (and VLA interaction with throwing cleanup), and other
        unvalidated automatic cleanup-plan shapes remain outside this subset.
    - [x] Extend partial construction tracking to bounded reference-bound
          struct prvalues with direct class members. Keep the hidden owner
          separate from the reference declaration that stores its cleanup
          plan; register each completed direct member during initialization,
          unregister those actions when the complete object is ready, and let
          the complete plan own normal cleanup. The throwing-member and normal
          destruction-order regression passes with 57 functions and zero
          fallback on i686/AMD64 at `-O0`/`-O2`, plus generated AMD64 and
          legacy-host execution in `test-verified-cxx-throwing-temporary`.
    - [x] Extend bounded reference-bound partial construction to flat,
          non-virtual direct bases with non-throwing aggregate initialization.
          Place two destructor-bearing bases behind a non-empty trivial base
          and verify their nonzero offsets, reverse-order unwind on a later
          throwing member, and handoff to the complete-object cleanup plan on
          success. The
          `test-verified-cxx-throwing-temporary` gate remains at 57 functions
          with zero fallback for i686/AMD64 at `-O0`/`-O2`, and generated
          AMD64 objects plus the legacy host verify failure and normal order.
    - [x] Bind a reference directly to a constructor-created prvalue with
          multiple non-virtual direct bases. A throw from the second base
          destroys the completed first base, a throw from the first destroys
          no incomplete subobject, a constructor-body throw destroys the
          completed member and bases, and success transfers lifetime to the
          reference scope. `test-verified-cxx-throwing-temporary` emits 57
          functions with zero fallback on i686/AMD64 at `-O0`/`-O2`; generated
          AMD64 objects and legacy-host execution verify all four paths.
    - [x] Permit a potentially throwing scalar call while evaluating a later
          base/member constructor argument when its own arguments are
          non-throwing and require no temporary cleanup. If the second-base
          argument throws before that base constructor begins, the first base
          is unwound exactly once (`6701`). The 57-function
          `test-verified-cxx-throwing-temporary` gate passes with zero fallback
          on i686/AMD64 at `-O0`/`-O2`, with verified-object and legacy-host
          execution.
    - [x] Verify a normal scope-exit cleanup whose `noexcept(false)` destructor
          throws into the surrounding handler exactly once. The
          `test-verified-cxx-throwing-temporary` gate emits 57 functions with
          zero fallback on i686/AMD64 at `-O0`/`-O2`; generated AMD64 and
          legacy-host execution both observe result `889` and cleanup order `9`.
- [x] target-independent MIR
  - [x] virtual register/block/phi/callを持つscalar MIRとIR→MIR shadow lowering/verifier
  - [x] critical-edge分類とcycle-safe parallel-copy schedulingによるphi edge lowering
- [x] bounded constant propagation / folding
  - [x] `-O1..3`での型範囲を守るAST整数constant foldingと短絡式除去
  - [x] 8/16/32/64-bit unsigned modulo演算・shift・比較・narrow cast folding
  - [x] alias/control-flow barrier付きblock-local整数constant propagation
  - [x] Preserve lvalue arguments bound to reference parameters of constructor
        compound initializers during constant propagation, and mark those
        locals escaped for dead-store analysis. The class-template reference
        member regression passes at `-O2` in `test-cxx-class-template-methods`.
  - [x] typed SSA整数演算・比較・castのconstant folding
  - [x] typed SSAの同一定数PHI／定数条件SELECT foldingとverifier/native回帰
  - [x] 変更・escapeのない局所整数に限定したwhile/do-while/forの
        loop-invariant constant propagationと両arch実行回帰
  - [x] side-effect-free integer algebraic identities (`+0`、`-0`、`*1`、`/1`、
        bitwise identity、zero folding)を型互換性と副作用保持付きで実装し、
        両archの最適化・実行回帰へ接続
  - [x] typed-SSAで同一整数値の`x - x`／`x ^ x`を0へ、`x & x`／`x | x`を
        元の値へ変形し、8/16/32/64-bitのverifier回帰で値置換と定数化を検証
  - [x] typed-SSAの符号付き／符号なし`x % 1`を0へ定数化し、除数0などの
        未定義・条件依存ケースを保持したまま8/16/32/64-bitで検証
  - [x] unsigned integerの`x * 2^k`／`2^k * x`、`x / 2^k`、`x % 2^k`を
        型付きshift/maskへstrength reductionし、i686/x86_64の即値shift生成と
        実行回帰を追加。signed/overflow-sensitive formは変更しない
  - [x] typed SSAの符号付き`SDIV`／`SREM`も定数±2冪をsign-fill biasと
        arithmetic shiftでlowerし、全width 8/16/32/64のIR interpreterで
        8-bit全入力、境界値と4096 deterministic inputsを照合。0、-1、非2冪は
        保持し、`INT_MIN` divisorを含むIR verifier回帰を追加
   - [x] typed SSAのunsigned `/`／`%`を、非ゼロ定数2冪についてlogical shift／maskへ
         削減。8/16/32/64-bit全幅で上位bit divisor、8-bit全入力、広幅の境界値と
         4,096 deterministic inputsを検証し、0と非2冪divisorは元の演算に保持。
         `rcc_ir_optimize_function(O1)`経由で`test-ir`のi686/AMD64 host runnerをgate
- [x] mem2reg、DCE、CSE/GVN
  - [x] 定数`if`分岐選択とゼロ回`while/for`のAST dead-code除去
    （`goto`および`case/default`からのentryを保持）
  - [x] block内control transfer後の直列DCEとlabel/case entry保持
  - [x] 未使用の副作用なし式文DCEとcall/volatile/C++ cleanup保持
  - [x] escape/control-flow barrier付きblock-local整数dead-store除去
  - [x] side effectのない未使用SSA定義の再帰的除去
  - [x] alias-free整数・cast・GEP・select・symbol addressのbasic-block内CSE
  - [x] dominator scopeと兄弟分岐隔離を持つSSA global value numbering
  - [x] 同一block内の同一SSA address・同一型のnon-volatile loadをCSEし、
        互いに別のdirect allocaと証明できるstoreだけを跨ぎ、それ以外の
        store/call/volatile access、型違い、block境界では再利用しないことを
        IR verifier regressionで検証
  - [x] 同一block内でdirect allocaへの直近store値を同じ型のnon-volatile loadへ
        forwardingし、unknown alias store/call/volatile loadでは全事実、
        volatile storeでは対象slotの値事実を失効させるIR verifier regressionを追加
  - [x] direct data symbolへの直近store値も同一symbol・同一型の後続loadへ
        forwardingする。global writeは他のmemory factを保守的に失効させ、
        unknown alias store/call/volatile accessでは転送しないことをIR regressionで検証。
        `test-ir`とC/C++の`test-optimize`を実行
- [ ] General loop transformations and cost-aware/interprocedural inlining
  - [x] 同一basic block内の同一direct alloca・同一値型への未観測の上書きstoreを
        除去し、forward済みreadの値を保持する。derived-pointer read/write、
        volatile read、call、型幅違いでは古いstoreを保持し、i686/AMD64のIR
        verifier回帰と`test-optimize`で検証
  - [x] 副作用なし・引数なし・単一整数returnの直接呼出しをO1で限定inline
  - [x] 副作用なし・単一整数returnの純粋整数式を最大8個の引数へ展開し、
        各引数の評価を一回に限定したO1 inlineと両arch実行・call除去回帰
  - [x] 一意なpreheaderを持つ自然ループに対して、純粋typed-SSA命令の
        ループ不変性を支配関係とuse-defで検証してpreheaderへ移動する限定LICMを
        O2/O3へ接続し、移動後のverifierとIR回帰を追加
  - [x] O3の各simplify反復後にLICMを再実行し、trivial header PHIの簡約で
        新たに不変となる式を次の固定点でhoistする。direct IR regressionでO2は
        loop内に保持しO3だけhoistすることを検証し、`test-ir`／`test-optimize`を実行
  - [x] 外部入口が複数ある自然ループで、安全なinvariantと入口を支配するoperandが
        ある場合にLICM preheaderを合成する。header phiの入口値が異なるときは
        preheader phiへ集約し、loop backedgeを保持してCFGを再解析する。phiなし／ありの
        複数入口・branch edge・hoisted value・verifierをdirect IR回帰で検証
  - [x] typed SSAの不変な左シフトは、定数shift幅が結果bit幅未満の場合だけ
        LICMでhoistし、動的または幅外のshift量はloop内に残す回帰を追加
  - [x] 範囲証明のない`fptrunc`／`fptosi`は、overflow・NaN・整数型範囲外の
        入力で無効となる可能性があるため、loopが0回の場合にも実行されるhoistを
        禁止する。direct IR regressionで両変換をloop bodyに保持し、`test-ir`と
        `test-optimize`で検証
  - [x] LICMで、単一のdominantな同型storeにより事前初期化され、loop内storeがなく、
        volatileでなく、address escapeのないdirect alloca scalar loadをpreheaderへ
        hoistする。allocation sizeとload widthも検査し、未初期化・volatile・loop write・
        escaping callは保持するtyped-SSA回帰を追加し、native-Windowsのx86/x64
        `ir_mem2reg_test` variantsで検証
  - [x] 副作用のない単純整数識別子／リテラル引数が関数本体で複数回参照される
        場合も、複雑式のAST共有は行わず安全にO1 inlineし、両archでcall除去と
        実行結果を回帰検証
  - [x] 宣言順に依存しない固定点反復で純粋スカラーinline候補の前方呼出し
        チェーンを解決する。成功したscalar expansionはeligible callを新生せず、
        caller単位の累積予算でcode growthを制限するため、固定8回の上限は不要。
        12段forward-wrapper chainの全call除去と両arch実行を`test-optimize`で検証。
        再帰・aggregate・exception callはこのpassの対象外として保持。
  - [x] 固定点反復を通じたcaller単位の累積展開予算を32 AST nodesに制限し、
        多数の小さなinline callでもcaller全体のcode growthを抑制。48 call-siteの
        fixtureでbudget内外の部分展開と残存callをi686/AMD64 objectで検査し、
        AMD64 SysV host executionおよび`test-optimize`を確認
  - [x] 副作用のない整数引数がinline本体で複数回参照される場合も、対応する
        式木を複製し、引数ごとの展開コストを16ノード以内に制限してO1 inlineへ
        接続。i686/x86_64のcall除去と実行結果を回帰検証
  - [x] 副作用のない整数return式のcast／条件演算子を式木複製の対象へ拡張し、
        型情報を保持したO1 inlineとi686/x86_64のcall除去・実行結果を回帰検証
  - [x] 純粋整数inlineのreturn式全体を64ノード以内に制限し、複数の引数置換で
        上限を迂回しない展開コスト計算と、上限超過時のcall保持を回帰検証
  - [x] 純粋scalar inlineをポインタ戻り値・ポインタ引数・ポインタ加算へ拡張し、
        引数の副作用を複製せず、i686/x86_64のcall除去と実行結果を回帰検証
  - [x] 同じ純粋scalar境界でポインタ間接参照と固定添字アクセスを扱い、
        `*p`／`p[i]`アクセサのcall除去・実行結果を両アーキテクチャで検証
  - [x] C++20のCリンケージ境界から同じ純粋scalarアクセサ群をコンパイルし、
        C++フロントエンド経由のO0/O1・i686/x86_64 call除去と実行結果を回帰検証
  - [x] 純粋scalar inlineで構造体ポインタの`p->field`と`(*p).field`を扱い、
        解決済みフィールド情報を保持したまま両archのcall除去・実行結果を検証
  - [x] ポインタinlineの副作用引数（`((*p += 1), p)`）を拒否してcallを保持し、
        引数の更新が一度だけ実行されることをC/C++・両archで回帰検証
  - [x] 引数付き純粋scalar inlineの形状判定で`sizeof`／`alignof`／`noexcept`を
        受理し、`sizeof *p`ラッパーの両arch call除去と実行結果を回帰検証
  - [x] C++20専用の`noexcept(value)`純粋ラッパーを実オブジェクトで検証し、
        C++ O0/O1のcall保持／除去と非評価結果を両archで回帰検証
  - [x] 不変文字列リテラルを返す純粋scalarラッパーを受理し、データ領域の
        実行検証はこの検査対象外と明記したうえで、両arch/C++のオブジェクト
        call除去を検証
  - [x] 副作用のない組み込みカンマ式を純粋scalar inlineでcloneし、`(value,
        value + 1)`ラッパーのC/C++・両arch call除去と実行結果を回帰検証
  - [x] 純粋scalar inlineのリテラル境界に文字リテラルを追加し、`value + 'A'`の
        C/C++・両arch call除去と実行結果を回帰検証
  - [x] compile-timeに1回だけ実行される副作用なし`for`を、`break`／`continue`／
        `goto`／labelを含まないことを確認してblockへbounded unrollし、両archの
        code-sizeと実行結果を回帰検証
  - [x] 定数初期値・定数境界で必ず0回になる副作用なし`for`を、符号付き／符号なし
        整数の比較規則を保持してinitializerだけのblockへ縮約し、両archの
        code-sizeと実行結果を回帰検証
  - [x] 定数`while (0)`の`do`本体を、loop-transferを含まない場合だけ一回実行の
        bodyへ縮約し、`continue`を含む本体は保持したまま両archの実行を回帰検証
  - [x] 初期値が局所定数として確定するC17 `while (i < literal)`／`<=`／`!=`の
        単位増減bodyを1〜4回へbounded unrollし、body内の宣言・label・loop transfer・
        control escape・induction別変更を除外して両archのcode-size比較と実行を回帰検証
  - [x] 初期値が局所定数として確定するC17 `do-while`の`<`／`>`／`!=`単位増減bodyを
        1〜4回へbounded unrollし、wrap・方向不一致・body内の宣言・label・loop/control
        transfer・induction別変更を除外してi686/AMD64のcode-sizeと実行を回帰検証
  - [x] C17の`while`／`do-while`の定数stride解析を±4から非ゼロの符号付き
        `int64_t`全域へ拡張し、`INT64_MIN`を含むoverflow・方向・`!=`到達性を
        未定義の符号反転なしで証明できる場合だけ1〜8回展開する。`INT64_MAX`を
        超える符号なしstrideは保持する。C17/C++20の大stride・最小負stride・
        未到達`!=`をi686/AMD64 object構造と実行で回帰検証
  - [x] 定数初期値・境界で2〜4回と確定できるC17 `for`を、符号付き／符号なし
        比較とincrement overflow境界を確認した上で、宣言・label・loop-transfer・
        C++ cleanupを含まないbodyだけbounded unrollし、i686/AMD64のcode-sizeと
        実行回帰を追加
  - [x] `i += 1`／`i = i + 1`（`1 + i`を含む）をunit-step inductionとして
        同じzero-trip／2〜4-trip proofへ接続し、volatile induction variableは
        observable accessを壊さないようunroll対象から除外して両arch回帰を追加
  - [x] `--i`／`i -= 1`／`i = i - 1`をdescending unit-step inductionとして
        signed／unsignedのzero-trip／2〜4-trip proofへ接続し、両archの
        code-sizeと実行回帰を追加
  - [x] 定数境界の`i != bound`をunit-step inductionのzero-trip／2〜4-trip
        proofへ接続し、正方向・逆方向の両arch code-size／実行回帰を追加
  - [x] 既存の整数変数を`for (i = literal; ...)`で初期化するC17形式を、
        宣言初期化と同じzero-trip／1-trip／2〜4-trip proofへ接続し、両archの
        code-sizeと実行結果を回帰検証
  - [x] C17 `for`のconstant-stride proofを±4から非ゼロの符号付き`int64_t`全域へ
        拡張し、signed／unsigned inductionのoverflow・方向・`!=`到達性を保持して
        証明済み1〜8-tripだけ展開する。`INT64_MIN`の安全なstep magnitude計算と
        `INT64_MAX`超の符号なしstride／到達不能`!=` loopの保持をC17/C++20
        i686/AMD64 object構造・実行で回帰検証
  - [x] C17 `for`の5-trip／8-tripを展開し、9-tripは8回上限を越えるため
        loop control flowを保持する境界回帰を追加。i686/AMD64のO0/O1 objectで
        local-label数を比較し、生成AMD64 codeを実行。Native Windows runnerも
        `VirtualAlloc`/`VirtualProtect`で実行し、引数付きSysV関数の呼出規約を検証。
        `test-optimize`は既存の`test-ci` production gateに含まれる
  - [x] side-effect-free floating literalからrepresentableなsigned／unsigned
        integerへのcastをO1でtruncation semanticsを保って定数化し、NaN・
        infinity・範囲外の変換はbackendへ残す。i686/AMD64のsize、実行、
        範囲外retention回帰を追加
  - [x] side-effect-free unsigned multiplication by 3／5／6／7を、剰余算術を
        保ったshift/addまたはshift/subへ強度削減し、signed式・副作用式は
        変換対象外のままi686/AMD64の`imul`除去と実行回帰を追加
  - [x] side-effect-free unsigned multiplication by 9〜127をnon-adjacent
        signed-digit shift/add/subへ強度削減し、C/C++ i686/AMD64の代表係数で
        `imul`除去、生成物検査、実行回帰を追加
  - [x] 同じ剰余幅を保つsigned-digit loweringを129〜255へ拡張し、129／255と
        `UINT32_MAX` wraparoundをC/C++ i686/AMD64で実行検証。255のO1 objectが
        明示的binary shift/add参照より短いことも比較し、signed式・副作用式を保持
  - [x] 係数256以上もNAF項数が3以下の場合に限って対象unsigned型の幅まで
        shift/add/subへ変換し、denseな32-bit／64-bit係数は`imul`を維持。
        32-bit境界係数と64-bitの`2^63±1`を含む境界・4,096 deterministic入力を
        AMD64で実行し、i686生成物の命令を検証
  - [x] C17/C++20のside-effect-free signed `/ -2^k`／`% -2^k`を、除算の
        ゼロ方向丸め・剰余符号を保つ算術shift/maskへ削減。i686/AMD64のO0/O1で
        境界値と4,096 deterministic inputsを実行し、side-effect dividendの一回評価と
        optimized `idiv` removalを`test-optimize`で検証。Windows native runnerも
        ABI-aware SysV function pointerでAMD64 C/C++ O1 objectsを実行
  - [ ] 一般のloop transformation、recursive/cost-aware inline、aggregate/exception
        callのinline
- [x] `-O0..3`ごとのpass pipeline
  - [x] O0検証のみ、O1 mem2reg/fold/DCE、O2 GVN追加、O3固定点反復
  - [x] rcc/rcc++共通の厳密な`-O0..3` CLI検証と範囲外fail-closed
- [x] function-wide MIR register allocation
  - [x] phi edge/call crossing対応MIR live intervalとpolicy駆動linear-scan/spill配置
  - [x] 非レイアウト順successor/back-edge対応CFG fixed-point liveness
  - [x] DIV/REMのAX:DXと可変shiftのCXを命令位置だけ予約するfixed-register制約
  - [x] function-wide interference-graph list coloring with call/fixed-register
        constraints and use-count-weighted spill selection; prove a call-crossing
        value can reclaim its sole callee-saved register without spilling on both
        architecture test builds

## 5. backend

- [x] MIR allocation/phi planからのdual-arch scalar machine IR instruction selectionとcritical-edge split verifier
  - [x] abstract physical registerから実x86 encoding registerへのSysV ABI mapping
  - [x] native-width DIV/REMのAX:DX sequenceと可変shiftのCX/two-address legalization
  - [x] integer/pointer parameter ingress、cycle-safe call argument、stack argument、AX return legalization
  - [x] callee-saved保存表とi686/AMD64 stack alignmentを含むprologue/epilogue frame plan
  - [x] native integer binaryのx86 two-address化と右辺alias退避
  - [x] prologue/epilogue、copy、immediate、binary、branch、REL32 callの初期native encoder
  - [x] native encoder出力の`.ro v2` `.text`/symbol/REL32 relocation bridge
  - [x] 16/32/64-bit unsigned/signed DIV/REMと可変shiftのnative encoding
  - [x] integer compare/setccとtruncate/zero/sign extension/reinterpret encoding
  - [x] stack addressとnative-width indirect load/store encoding
  - [x] scaled GEPとscalar selectのalias-safe encoding
  - [x] C/C++の明示option付きverified `.ro v2` production経路、pointer更新・ptrdiff・短絡phi・switch・単一lvalue評価の両arch実行検証、translation-unit fallback
  - [x] C17/C++20 global `_Thread_local`/`thread_local` のread/write/updateを
        typed SSAからi686/AMD64 local-exec TLS addressへlowerし、`-O2`で
        translation-unit fallbackなし、
        `.ro`の`.tls`/`BIND_TLS`/`TLSOFF32S`とFS/GS thread-pointer命令列、
        C/C++間のTLS importを含むRLD後のRIN v3を`rinvalidate`が受理することを検証
  - [x] direct RCC／RLD-linkedのTLS RIN/RLLで、ABI minorが
        `RIN_IMAGE_ABI_MINOR_UNWIND_FORMAT`以上の場合にUNWIND sectionの
        `READ | UNWIND_DWARF_DEBUG_FRAME` flagsとfile boundsを検査し、
        `test-tls`およびC17 aggregateで回帰確認
  - [x] `__builtin_expect`を副作用順序付き値伝播へ、`__builtin_unreachable`／
        `__builtin_trap`をtyped-SSA終端と実UD2へlowerし、i686/AMD64の
        verified backend fallbackなし回帰を追加
  - [x] `__builtin_bswap16/32/64`をtyped-SSAのmask／shift／論理演算へlowerし、
        x64の16/32/64-bit実行値、i686の16/32/64-bit fallbackなしを
        回帰検証
  - [x] typed SSAで`float`／`double`リテラルをread-only constantからloadし、
        C/C++ x86_64 SysV aggregate fixtureの`-O0`/`-O2`実行で値を検証
  - [x] x86_64 typed SSAで`float`／`double`の加算・減算・乗算・除算を
        SSE scalar命令へlowerし、混在精度変換、FPR register pressureとstack引数を
        含むC/C++ `-O0`/`-O2` objectのfallbackなし生成・runtime実行を検証。
  - [x] x86_64 typed SSAでscalar floating comparisonとtruth conversionを
        実装し、C/C++ `-O0`/`-O2`でNaN時の`!=`／ordered comparison、
        `+0`/`-0`の比較・truth、stack引数とFPR spill後のruntime結果を検証。
  - [x] x86_64 typed SSAでfloating unary minus、前置／後置`++`/`--`、
        `+=`/`-=`/`*=`/`/=`を実装し、C/C++ `-O0`/`-O2`でfallbackなしの
        runtime結果を検証。
  - [x] x86_64 SysVでtrivialな単一SSE-eightbyte aggregate（`float`／`double`
        一field struct）のparameter、XMM0 return、call-result storage、条件式の
        branch selectionをtyped SSAへlowerし、C/C++ `-O0`/`-O2`でmember抽出を含む
        実行とsigned zero／infinity／NaN payloadのbit-exact保存をfallbackなしで
        検証。nontrivial conditional、mixed-classおよび複数eightbyte returnは未対応。
  - [x] typed SSAのi686 scalar floating arithmeticをx87 register-stack規約と
        cdecl引数／戻り値ABIに合わせて実装し、C/C++の`-O0`/`-O2`を実機以外で
        fallbackなし実行検証する。
    - [x] x87 typed arithmetic実装前のi686でfloat/double型をlowering前に
          legacy backendへ戻し、最適化で定数式になった関数がx86 selectorで
          late failureしないことをC/C++ `test-optimize`で検証した。
    - [x] typed SSAのi686 scalar `float`/`double`比較とtruth conversionを
          `FUCOMIP`のunordered flagsを保ってlowerし、NaNの`!=`/ordered predicate、
          ±0、真偽値をC/C++ `-O0`/`-O2`のfallbackなし実行で検証する。
    - [x] typed SSAのi686 scalar integer/floating conversion、unary、compound
          update、conditional selectをx87 semanticsへlowerし、C/C++
          `-O0`/`-O2`でfallbackなしのobject・runtime結果を検証する。
  - [x] i686 wide-scalarの代入、複合代入、pre/post incrementをpair
        load/storeとcarry/borrow付き演算へlowerし、両archのobject・x64
        実行回帰で検証
  - [x] i686 wide-scalarのtruth、論理否定、short-circuit AND/ORを既存の
        two-word truth reductionとCFGへ接続し、両archのobject・x64実行で検証
  - [x] wide-scalar戻り値を持つdirect variadic callでi686のinteger/pointer
        引数をSSA loweringし、narrow integerのC default promotionと64-bit
        引数のlow/high配置を含むi686/x64 object・外部call relocationを検証。
        aggregate variadic引数とx86_64 SysV `va_arg`関数本体は対象外
  - [x] i686 cdecl variadic callで`float`引数を既定の`double`へ拡張し、
        calleeの連続する`va_arg(double)`が8-byte stack slotを読む経路をtyped SSAへ
        接続。2個のfloat引数を使うC regressionでi686/AMD64 `-O0`/`-O2`の
        verified objectがfallbackなしで生成され、対応hostではobject実行も検証。
        WindowsではAMD64 verified object実行とi686 legacy ABI controlを実行
  - [x] i686 cdecl variadic calleeの`va_start`を最終named parameterのincoming
        stack slotからlowerし、pointer-based `va_copy`／`va_end`、default
        promotion後のinteger、pointer、two-word 64-bit `va_arg` loadをtyped SSAへ
        接続。i686 verified backendでfallbackなしのobject生成を検証し、x64では
        完全なlegacy backendへの明示fallbackを保持
  - [x] i686 cdecl／x86_64 SysVの両方で、7個のnamed GP parameterに続く可変引数の
        `va_start` cursorを生成し、両architectureのverified objectでfallbackなしを確認。
        x64 host runtimeで最初のstack overflow variadic argumentが読めることも確認
  - [x] x86_64 SysV variadic calleeのtyped-SSA経路で、整数／pointerのnamed GP
        scalar parameter（6個超を含む）に対する`gp_offset`、176-byte GPR/XMM
        save area、named parameter数に応じたoverflow stack cursorを実装。
        local array `va_list`の`va_copy`／`va_end`、integer/pointer `va_arg`の
        register・stack両経路、および7個目のnamed parameter後のoverflow開始位置を
        `-O0`/`-O2` runtime bridgeで検証。variadic callにおけるdefault integer
        promotionも検証
  - [x] 配列型`va_list`を引数調整して受け取る非variadic helper内で、pointer経由の
        `va_arg`、`va_copy`、`va_end`をlowerし、copy側の読み取りがsource cursorを
        変えず、helper側の消費がcallerへ反映されることをx64 runtimeで検証
  - [x] C++ `va_list&` helperのtyped-SSA loweringを確認し、参照経由の
        `va_copy`がsource cursorを進めず、参照先での`va_arg`がcallerのcursorを
        進めることを検証。i686/AMD64 `-O0`とAMD64 `-O2`のverified objectで
        fallbackなし、AMD64 host executionで結果90を確認
  - [ ] Extend typed-SSA x86_64 SysV variadic coverage beyond the verified
        scalar-FP and bounded aggregate profiles below. Remaining work is
        nontrivial/unsupported aggregate layouts and adjusted-`va_list` shapes;
        unsupported functions must continue through the complete legacy backend.
      - [x] Lower scalar `double va_arg` from XMM save slots and the overflow
        area, advance `fp_offset`/overflow independently, and initialize the
        cursor after named FP parameters. Verify first, second, ninth/stack,
        named-double, and mixed GP/SSE overflow cases at `-O0`/`-O2`, with no
        verified-backend fallback and host SysV execution.
      - [x] Lower trivial aggregate `va_arg` values classified across INTEGER,
        SSE, and MEMORY classes, including mixed GP/SSE retrieval, whole-object
        stack fallback when a register bank is short, and aligned MEMORY-class
        overflow. Verify C and C++ objects and host ABI execution at `-O0`/`-O2`
        without fallback for supported layouts.
      - [x] Classify supported named scalar FP and trivial aggregate parameters
        at function entry and `va_start`, including independent GP/XMM cursors,
        register exhaustion, and whole-stack aggregate placement. Verify C/C++
        call/entry paths and variadic cursor results through the verified
        backend at both optimization levels.
      - [x] MEMORY-class aggregate `va_arg`の24-byte C/C++構造体と、
        16-byte alignasを持つ32-byte C++構造体を実装に接続し、stack spill後の
        overflow cursor／alignmentを含むSysV host ABI実行を`-O0`/`-O2`で検証。
        全対象関数がfallbackなしでtyped SSAから生成されることも確認。
      - [x] 1 INTEGER eightbyteに収まるtrivial C aggregateのvariadic callを
        typed SSAへmarshalし、同じobject内の`va_arg` calleeまで接続。C callerから
        `{13, 7}`を渡した結果137を、`-O0`/`-O2`かつfallbackなしで検証。
      - [x] trivial C aggregateのvariadic callを最大2個のINTEGER eightbyteまで
        拡張し、両wordがregisterに入る場合と両wordがstackへ送られる場合を実行検証。
        1 GPRだけ空いた境界ではaggregate全体をstackへ配置し、残りのGPRを後続引数に
        保持する経路もC/C++ `-O0`/`-O2`でfallbackなし実行確認。
      - [x] single SSE eightbyteに分類される単一`float`／`double` field aggregateを
        variadic call-siteからmarshalし、C/C++ objectでXMM register、最後のXMM register、
        SSE register枯渇後のstack配置を`-O0`/`-O2`実行検証。
      - [x] 2個のSSE eightbyteを持つtrivial aggregateを両XMM registerへ渡す経路と、
        SSE bank枯渇後に両方stackへ置く経路と、残り1 XMM slotでは全体をstackへ置いて
        XMM cursorを保持するstraddle経路をC/C++ `-O0`/`-O2`でfallbackなし実行検証。
      - [x] 最大2個のINTEGER/SSE eightbyteで構成されるmixed GP/SSE aggregateを、
        必要な両register bankに余裕がある場合と両bank枯渇後のstack配置でmarshalする。
        一方のbankだけが不足する境界でもaggregate全体をstackへ配置し、残るregister bankを
        後続引数に保持する。C/C++の`-O0`/`-O2`でregister／全stack／片bank straddleを
        fallbackなしでruntime検証。
      - [x] nonvariadic x86_64 SysVのnamed trivial aggregate parameterをeightbyteごとの
        INTEGER/SSE classでfunction entryとcall-site双方に分類する。C/C++の`-O0`/`-O2`で
        register配置と全体stack配置を実行検証し、register straddleではaggregate全体をstackへ
        配置する。mixed GP/SSEのregister配置、両bank exhaustion後のstack配置、片bank straddleを
        fallbackなしで実行確認。
      - [x] variadic functionのnamed trivial aggregate parameterについて、GP/XMM
        register配置と両bank exhaustion時のwhole-stack配置をfunction entry、call-site、
        `va_start` overflow位置まで接続。C/C++の`-O0`/`-O2`でregister／stack後の
        `va_arg(int/double)`をruntime検証し、fallbackなしで実行。
      - [x] variadic call-siteから24-byte MEMORY-class aggregateをtyped SSAで
        by-value stack marshalし、C/C++の`-O0`/`-O2`でcalleeの`va_arg`結果を
        fallbackなしでruntime検証する。
      - [x] 32-byte aligned 32-byte C/C++ aggregateを9個目のstack double後に
        `va_arg`し、さらに24-byte MEMORY aggregateを続けて読む経路をtyped SSAで
        lowerし、call loweringによる複数のMEMORY descriptor stack配置とcalleeの
        overflow cursor/alignment更新をfallbackなしで接続する。9個のdoubleを消費した後の
        aggregate fieldと後続aggregate fieldの値をC/C++ `-O0`/`-O2` runtimeで検証する。
        unsupported floating-point binary arithmeticをfixtureから分離し、対象関数自身が
        typed SSAで出力されることをfallback reason検査でも固定する。C/C++の`-O0`/`-O2`
        全4 objectでfallbackなし、host SysV executionで結果100を確認。
      - [x] 64-byte aligned 64-byte C/C++ MEMORY aggregateを9個目のstack double後に
        variadic callerから渡し、calleeの`va_arg`で読み取った後に24-byte MEMORY
        aggregateを読む経路をtyped SSAで接続する。`test-verified-backend`のC/C++
        `-O0`/`-O2`全4 objectでfallbackなし、host SysV executionで結果100を確認。
      - [x] 最大4096-byte alignmentのC/C++ variadic MEMORY aggregateを9個目のstack
        double後に渡し、calleeの`va_arg`で読んだ後の24-byte aggregateも正しく読む。
        C/C++ `-O0`/`-O2`全4 objectで57関数をfallbackなしで生成し、host SysV実行で
        結果100を検証。clang64の実行ファイルを`CLANG`に指定して`test-verified-backend`
        全体も再実行し、dynamic-TLS、verified backend、production object各gateが成功。
      - [x] C/C++ `alignas(8192)`/`_Alignas(8192)`のvariadic MEMORY aggregateを
        9個目のstack double後に渡し、さらに24-byte aggregateを`va_arg`で読む経路を
        caller/callee双方でtyped SSA loweringする。C/C++ `-O0`/`-O2`全4 objectは
        fallbackなしで61関数を生成し、host SysV実行で結果100を検証。
      - [x] 128/256/512/1024/2048-byte alignmentのC/C++ variadic MEMORY aggregateを
        9個目のstack double後に連続して渡し、さらに24-byte aggregateを読む。C/C++
        `-O0`/`-O2`の全4 objectで57関数をfallbackなしで生成し、verified SysV object
        実行を各objectで確認（結果100）。
        現在のsource progress: 対応するinteger/pointer/float/double aggregateの`va_arg`は、
        INTEGER/SSE eightbyte分類、register-save／stack fallback、16-byte stack alignment、
        16-byteを超えるMEMORY aggregateを実装・実行検証した。variadic callerは最大2 INTEGER
        eightbyte、最大2個のhomogeneous SSE eightbyte、または最大2個のmixed INTEGER/SSE
        eightbyteを持つtrivial aggregateに限定される。nonvariadic named aggregate parametersも同じ
        bounded INTEGER/SSE分類でregister／whole-stack loweringと実行検証を完了し、variadic
        functionのbounded named aggregateもregister／whole-stack配置と`va_start` cursorを
        C/C++ O0/O2で実行検証した。片bankだけのregister straddleもwhole-stack assignmentで
        typed verified backendから実行する。32/64/128/256/512/1024/2048/4096-byte
        alignmentのvariadic callerはC/C++ `-O0`/`-O2`でfallbackなし生成・実行検証済み。
        8192-byte alignmentについても独立したC/C++ caller/callee fixtureを実行検証した。
        その他のalignment値は網羅していない。
        backendの上限8192 byteを超えるalignment、bounded profile外のnamed aggregate layouts、および
        残るadjusted-`va_list`形態は未実装であり、このparent checkboxは未完了のままにする。
  - [x] x86_64 SysVで`va_list*`を受け取るhelperの`va_arg(*p, T)`／
        `va_copy(local, *p)`をtyped SSAへlowerし、copy側の消費がsource cursorを
        動かさず、pointer側の消費がcaller cursorを共有することを、i686/AMD64
        object生成とAMD64実行、O0/O2 verified-backend gateで検証
  - [x] C++20 frontendでもtarget別`__builtin_va_list`を初期化して標準
        `stdarg.h`の`va_list` typedefを解析し、`va_list*` helperをi686/AMD64の
        verified objectとx64 O0/O2実行で検証
- [x] i386基本integer/cdecl code generation
  - [x] 宣言量に基づくstack frameとbyte/word typed load/store
- [x] AMD64 SysV基本integer引数とscalar/小aggregate経路
- [x] basic global dataと`ABS32U/ABS32S/ABS64`
  - [x] direct RIN/NDRVのDATA/CODE symbol解決と関数ポインタ
  - [x] extern/tentative definitionとzero-file-size BSS
  - [x] 文字列literalのread-only RODATA分離
- [x] i386での完全な64-bit整数演算と戻り値
  - [x] EDX:EAX scalar return、8-byte引数、literal/cast/local/global/call基礎
  - [x] signed/unsigned比較とSHLD/SHRDによるwide shift
  - [x] multiply/divide/modulo
  - [x] 前置/後置increment/decrementと全integer compound assignment
- [x] i686/AMD64 SysV integer・pointer scalar variadic ABI
- [x] SysV aggregate分類と、boundedなfloating-point／aggregate variadic ABI
      profileをi686/AMD64で実装・検証。未対応layoutとadjusted `va_list`
      形態は上記の未完了項目に残す
- [x] i686 cdeclとAMD64 SysVのfloating scalar variadic引数について、既定昇格、
      XMM register-save領域、`va_arg`のregister/overflow経路を実装
- [x] bounded PIC/PIE、GOT/PLT、TLS relocation
  - [x] direct/internal、GOT/PLT、local-exec TLSの両arch relocation検証
  - [ ] 全visibility、interposition、TLS model、shared-library ABI互換性
- [ ] DWARF debug/unwind
  - [x] `-g`でrelocatable `.ro`へ関数開始・source file・lineを持つ最小DWARF
        `.debug_line`を出力し、両archのread/link回帰を追加
  - [x] codegenが記録したnested lexical blockの開始offsetを追加の
        `.debug_line` source rowへrelocation付きで出力し、関数入口だけに
        依存しないi686/AMD64 line-table回帰を追加
  - [x] 最小compile unit／subprogram DIE、`.debug_abbrev`／`.debug_str`、
        `low_pc` relocationを追加し、両archのobject/link回帰へ接続
  - [x] `DW_TAG_formal_parameter`／`DW_TAG_variable`へ実在するC/C++ stack
        declarationの名前とEBP/RBP相対`DW_OP_breg` locationを出力し、
        i686/AMD64 object・RLD link回帰で検証
  - [x] VLAのstack slotには動的配列のaddressではなくaddressを保持する
        pointerが格納されるため、VLA localのlocation式だけ`DW_OP_deref`を
        追加し、対象DIEのregister／負offset／opcodeを両archで検証
  - [x] verified IRのsource allocaから宣言を最終x86 frame layoutまで追跡し、
        EBP/RBP基準の実stack offsetをvariable/parameter DIEへ出力。位置を持た
        ない変数へ誤った`Decl.var_offset`を使わないことをi686/AMD64のO0/O2
        object regressionで検証
  - [x] C17/C++20の`-g -O2`でsource declarationに紐づくallocaだけをmem2reg
        から保持し、compiler temporaryのSSA昇格と残りの最適化は継続する。
        parameter/local/nested-local DIEが別々の実frame slotを指すことを
        i686/AMD64で検証
  - [x] stack declarationへ`DW_AT_type`を付与し、基本型・ポインタ型の
        v4 type DIEと、未対応の複合型をscalarと偽らないopaque DIEとして
        i686/AMD64 object回帰で検証
  - [x] stack declarationへ`DW_AT_decl_file`／`DW_AT_decl_line`／
        `DW_AT_decl_column`を追加し、関数・変数ごとのsource file tableを
        line/infoで共有してC/i686・C/x86_64回帰で検証
  - [x] 現行のframe-pointer ABIに合わせてsubprogramへ`DW_AT_frame_base`を
        出力し、i686/AMD64のEBP/RBP base expressionをdebug-info回帰で検証
  - [x] `inline`宣言を実インライン化済みと誤認せず、subprogram DIEへ
        `DW_AT_inline=DW_INL_declared_inlined`を記録し、非inline関数の
        `DW_INL_not_inlined`と両archのdebug-info回帰で検証
  - [x] C/C++のsource-level関数DIEへ関数型のprototype状態から
        `DW_AT_prototyped`を出力し、Cの`f(void)`／旧形式`f()`とC++
        static/non-static member関数の値をi686/AMD64で検証
  - [x] C++非staticメンバー関数の`DW_AT_name`にsource名を出し、mangled
        `DW_AT_linkage_name`を維持しながら、`DW_AT_object_pointer`と
        `DW_AT_containing_type`から人工`this` parameterと所有class DIEを参照し、
        i686/x86_64のobject回帰で両方の参照先を検証
  - [x] static C++メンバー関数でもsource/mangled名を保ち、object pointerを
        捏造せず`DW_AT_containing_type`から所有class DIEを参照する両arch回帰を追加
  - [x] C++ non-static/staticメンバー関数DIEへ`DW_AT_accessibility`を追加し、
      private/protected/publicをi686/x86_64の`.ro`回帰で検証
  - [x] C++ classの通常／bit-field member DIEへ`DW_AT_accessibility`を出し、
        private/protected/publicの3値をi686/x86_64双方のdebug-info回帰で検証
  - [x] subprogram DIEへ実在する関数戻り型の`DW_AT_type`参照を追加し、
        AST型を持たない生成関数には型を捏造せず型無しabbrevを選択する
        両archのdebug-info回帰を追加
  - [x] 現行のframe-pointer prologue／epilogueに対応するDWARF32
        `.debug_frame`のCIE/FDEをrelocation付きで出力し、CFA・saved FP・
        return address規則とloadable imageからの除外を両archで検証。非標準
        prologueの完全なCFIは引き続き未実装
  - [x] CIE/FDEのPC進行を実際の`push fp; mov fp,sp`完了位置へ合わせ、
        epilogueのCFA復帰とsaved FP復元を両archのdebug-info回帰で検証
  - [x] 静的参照一時オブジェクトのvtable設定中にEBP/RBPを一時scratchへ
        流用せず、保存したcaller-saved registerをbaseに使うようcodegenを変更。
        実行時virtual dispatchをi686/AMD64で検証し、frame pointerを維持する。
  - [x] 仮想基底を持つ多相temporaryへの静的参照も追加し、base subobject
        のvtable dispatchまで実行検証。i686/AMD64 codegenとAMD64 host実行に加え、
        GCC C++20 host fixtureもfocused targetから実行する。
  - [x] 非再帰のstruct／union／固定長array／vector型について、実フィールド・
        要素型・byte size・member locationを持つDWARF type DIEを出力し、
        i686/AMD64のobject・link回帰で検証。bit-fieldにはbit size／offsetも
        出力し、aggregate先頭からのdata bit offsetを記録する。
  - [x] 再帰struct／unionのmember type参照をDIE生成後のforward-reference
        patchで解決し、自己参照pointerが実aggregate DIEを指すことを
        i686/AMD64のobject debug-info回帰で検証
  - [x] 完全layout済みC++ classの直接基底を`DW_TAG_inheritance`で表し、
        non-virtual baseは固定subobject offset、virtual baseはRinOSの各subobject
        vbase pointer／relative-offset tableをたどる`DW_AT_data_member_location`
        expressionと`DW_AT_virtuality`で出力。派生型のflatten済み`TypeField`から
        継承メンバーを除外し、public/private複数継承、複数virtual-base table
        index、virtual baseを持つ型をnon-virtual intermediate経由で継承する経路を
        i686/AMD64で検証。未解決／不完全base layoutは引き続き未完
  - [x] source locationを持つtranslation-unit scopeのC/C++ `typedef`
        declarationへ`DW_TAG_typedef`、実基底型参照、file/line/columnを出力し、
        C/C++双方のi686/AMD64 debug-info回帰で検証。C++ namespace scopeの
        `using Alias = Type`とclass内の具体型aliasも`DW_TAG_typedef`として出力し、
        class内aliasのsource location／public・protected・private accessibilityを
        同じ両arch回帰で検証。関数scopeとnested lexical block内の具体型
        `using` aliasも、宣言したscope直下の`DW_TAG_typedef`として出力し、
        関数template instance内ではalias underlying typeもtemplate substitutionし、
        concrete aliasの実型・byte size・source location・scopeをi686/AMD64で
        検証。未instantiateのdependent aliasとunsupported template-definition
        parameter DIEは未完
  - [x] Concrete C++ class-template specializationsへ、実引数型を参照する
        `DW_TAG_template_type_parameter`と、signed/unsigned LEB値を保持する
        `DW_TAG_template_value_parameter`を出力。nested class type、正負のsigned
        値、unsigned 32-bit値をi686/AMD64の`test-debug-info`で検証。
        trailing class-template type packsと、non-dependent integral型を持つ
        signed/unsigned non-type packsはGCC互換DWARF v4
        `DW_TAG_GNU_template_parameter_pack`配下へ実引数ごとのtype/value DIEを
        出力し、empty／複数要素とsigned／unsigned値を同じ両arch gateで検証。
  - [x] Concrete class-template instantiationsのtemplate-template argumentを
        `DW_TAG_GNU_template_template_param`／`DW_AT_GNU_template_name`で出力し、
        globalおよびnamespace-qualified actual template名をGCC DWARF oracleと
        i686/AMD64の`test-debug-info`で検証
  - [x] Concrete class-template template-template parameter packsをparse・保持し、
        `sizeof...(Template)`の0/1/2要素、重複specializationのcache、namespaceを含む
        Itanium manglingをGCCと比較。DWARF v4のGNU parameter-pack DIEと各実template名
        をi686/AMD64 `test-debug-info`で検証
  - [x] dependent class template-template parameter packsを基底指定
        `Bases<int>...`と別class template-idへの`Templates...` forwardingの両方で実体化し、
        空／1／2要素のbase countとobject layoutをGCCと比較、i686/AMD64生成とAMD64実行で検証。
        sourceがpackでないexpansionとellipsis欠落もi686/AMD64でdiagnosticを検証
  - [x] 先行type parameterを宣言型とするconcrete signed／unsigned integral
        non-type packs（`template<class T, T... Values>`）をclass／function
        templateのDWARF parameter-pack DIEへ出力。empty／複数要素、`short`の
        負値、`unsigned short`の高bit値、およびsigned enum型値をi686/AMD64で
        検証。scalar enum NTTPもclass/function templateのDWARFとItanium
        manglingをGCCと比較。同じ値を異なるdependent型でspecializeした場合の
        class-instance cache分離も回帰化
  - [x] Named enum NTTPをconcrete class/function template scalar valueとして
        instantiateし、enum underlying type参照とsigned enumeratorをDWARFへ出力。
        enum型をunregistered conceptと誤認しないlookaheadを追加し、scalar／pack
        manglingのGCC一致、i686/AMD64 `test-debug-info`、named-concept回帰で検証
  - [x] `typename Owner::value_type... Values`と、二段の
        `typename Owner::value_type::nested_type... Values`をintegral non-type
        packの宣言型としてparseし、concrete alias chainの最終型をfunction-template
        manglingとclass／function DWARFへ反映。class consumerとfunction specialization
        のItanium symbolをGCCとbyte-for-byte比較し、Owner型、`short`型参照、-4/19の
        pack値をi686/AMD64 `test-debug-info`で検証。class/function dependent nested
        type、named-concept、template-template、non-type pack expressionの回帰も通過
  - [x] `typename Owner::value_type Value`をdependent scalar non-type parameterの
        宣言型としてfunction template instantiate時にも解決。class／function双方の
        specializationで`short`型と-6のDWARF値をi686/AMD64で検証し、GCCとItanium
        manglingを照合
  - [x] Function-template return/parameterにおけるdependent nested aliasのpointer／
        reference／member-pointer型を両architectureで解決し、owner名をASTへ安定保持。
        2段のdependent alias pathとmember-pointer引数も含め、GCC C++20とItanium
        symbolを照合し、i686/AMD64生成とgenerated x64実行を検証
  - [x] Function-template return／parameterのpointer／reference／member-pointer
        compound dependent declared typeをi686/AMD64のDWARF DIEで検証。
        `test-debug-info`全体が成功
  - [x] Function-template parameter packのdependent nested aliasをpointer／
        reference／member-pointerへ展開し、各要素の実型とmember-pointer ownerを
        i686/AMD64のDWARF parameter DIEで検証。GCC C++20とのItanium symbol一致も
        確認し、`test-debug-info`全体が成功
  - [x] Function parameter packのdependent nested alias expansionについて、
        by-value引数とfixed-bound array lvalue-reference引数を追加検証。
        `short`／`long`の異なるalias型を持つ2 ownerでi686/AMD64のparameter
        DWARF DIEとarray boundを検証し、GCC C++20とのItanium symbol一致および
        `test-debug-info`全体の成功を確認
  - [x] const lvalue-referenceを含むheterogeneous nested-alias function
        parameter pack expansionを追加し、i686/AMD64 formal-parameter DIEの
        reference -> const-qualified type -> alias base typeを検証
  - [ ] その他のdependent function parameter pack expansion、non-integral
        structural non-type packs、および未instantiateのdependent
        alias/template-definition DIEは引き続き未完
  - [x] Concrete C++ function-template instancesにtemplate ownerと実体化済みの
        type/value argumentsを保持し、subprogram DIE配下へtemplate type／
        integral value parameter DIEを出力。i686/AMD64の`test-debug-info`で
        `Value = unsigned short`と`Bias = -7`を検証。trailing type packおよび
        non-dependent integral型のsigned/unsigned non-type packもpack DIEと
        実引数ごとのchild DIEへ出力し、empty／複数要素、負値、高bit unsigned値を
        i686/AMD64で検証。directおよびmulti-hop qualified nested aliasを宣言型とする
        integral packもfunction DIEに実型・値を出力しGCCとmangling照合済み。
        pointer／reference等のcompound／non-integral dependent value packと、
        未instantiateのdependent function-template definition DIEは未完
  - [x] Function-template scalar non-type parameterを値expressionへ置換する際、
        dependent nested-aliasで宣言された実型も具体化してからexpressionへ設定。
        `static_cast<int>(Value)`を含むfunction specializationのi686/AMD64生成と
        GCC一致、全`test-debug-info`回帰で検証
  - [x] Concrete function-template template-template argumentを明示指定および
        deductionで解決し、type parameter／integral non-type parameter signaturesの
        compatible actual template名を保持。Itanium manglingとGNU DWARF v4
        `DW_TAG_GNU_template_template_param`をglobal／namespace-qualified名でGCCと比較し、
        `test-debug-info`のi686/AMD64回帰およびfunction-template overload orderingで検証
  - [x] enum型と列挙子へ`DW_TAG_enumeration_type`／`DW_TAG_enumerator`と
        signed／unsigned constant valueを出力し、unsigned full-width値を含む
        i686/AMD64のobject・link回帰で検証
  - [x] 非再帰function typeへ`DW_TAG_subroutine_type`と戻り型・parameter
        type DIEを出力し、再帰function typeのforward referenceもpatchで
        解決してi686/AMD64のdebug-info回帰で検証
  - [x] const／volatile／restrict／atomic修飾型を対応するDWARF qualifier
        DIEと実在する基底型参照へlowerし、i686/AMD64のglobal variableと
        linked-image debug-info回帰で検証
  - [x] C++ reference declaratorsを保持し、`DW_TAG_reference_type`／
        `DW_TAG_rvalue_reference_type` DIEから参照先型への`DW_AT_type`を
        出力。lvalue/rvalue reference parameterのDIEと参照先base typeを
        i686/AMD64の`test-debug-info`で検証
  - [x] C++ data/function member-pointer型を`DW_TAG_ptr_to_member_type`と
        実メンバー型・containing class型参照で出力し、member-functionの
        cv-qualified type DIEも含めてbyte sizeと参照先をi686/AMD64で検証
  - [x] nested compound statementへ`DW_TAG_lexical_block`を出力し、実際の
        `DW_AT_decl_file`／`DW_AT_decl_line`／`DW_AT_decl_column`とblock内
        local DIEの親子関係をi686/AMD64のdebug-info回帰で検証。block-to-PC
        rangeをcodegenのhalf-open code offsetから`DW_AT_low_pc`／
        `DW_AT_high_pc`へrelocation付きで出力し、両arch object回帰で検証。
        完全なinline attribution/CFIは引き続き未実装
  - [x] nested lexical block内のframe-backed localへDWARF v4 `.debug_loc`
        location listを出力し、blockのdiscontiguous PC ranges、各rangeの
        function-symbol relocation、frame-relative expression、VLA address
        dereferenceを検証。宣言statementのcode rangeが得られる場合はlocation
        list開始を初期化完了PCまでclipし、C legacyでscope開始PCより後になることを
        i686/AMD64のrelocation addend比較で検証。C legacy、C verified O0/O2、
        C++ verified O2の`.ro`回帰と、RLDがdebug-only sectionをloadable image
        から除外する回帰を`test-debug-info`へ接続。全制御フローでの正確な
        lifetime、register/piece locationsは未完
  - [x] verified backendで命令rangeのないframe-backed localのlocation listを
        同一lexical scope内の次の実PCから開始し、scope entryより後になることを
        i686/AMD64のC verified O0/O2で検証。scope末尾に後続PCがないlocalは
        empty `.debug_loc`や偽のframe locationを出さないことも同じ回帰で確認
  - [x] verified statement PC rangeを挿入時に昇順・disjoint unionへ正規化し、
        overlapと隣接rangeを統合。branch scope localのlocation listを
        i686/AMD64のO0/O2で検証し、`branch_value`の開始・終了PCが
        同一関数の外側local `result` のlocation lifetime内に厳密に収まることも確認
  - [x] `for`文を独立したDWARF lexical blockとして出力し、for-init localの
        `.debug_loc`をループ終了PCで閉じる。C legacy、C verified O0/O2、
        C++ legacy/verified O2のi686/AMD64 object回帰で、外側localより短い
        location lifetimeを検証
  - [x] legacy i686/AMD64 codegenで命令を出したstatementの開始offsetを
        source line rowへ対応付け、代入・分岐・returnの`.debug_line`行を
        objectからデコードして両archで検証
  - [x] verified encoderからcallee-saved GPR・実FP相対保存slot・保存完了PCを
        `.debug_frame`まで伝搬し、保存命令完了後だけ`DW_CFA_offset`を有効化。
        CIEにABI-preserved GPRの`DW_CFA_same_value`を定義し、`leave`後は
        CIE ruleへ戻す。non-terminal return後はstack-slot ruleを再適用。
        live-across-call回帰でi686/AMD64のO0/O2 CIE/FDEをdecodeして検証
  - [x] legacy i686/AMD64 codegenで実際に展開したbounded pure-scalar inline
        callへcalleeのabstract-origin DIEとcall-site PC range、宣言元・呼出元
        file/line/columnを出力。header定義を別sourceから呼ぶ`-g -O1`回帰で
        origin参照・PC relocation・DWARF file indexを両archで検証
  - [x] nested bounded inline expansionを親子の`DW_TAG_inlined_subroutine`
        DIEとして出力し、abstract-originにformal parameter DIEを含める。
        `test-debug-info`で両archのnested parent/child、PC range包含、宣言元
        parameterのname/type/locationを検証
  - [x] legacy i686/AMD64のinlined-subroutine DIEに、callee本体直下とnested lexical
        block内のautomatic const scalar localをabstract declarationとconcrete instanceに
        分けて出力。signed/unsigned integer・enum・float/doubleの保存済み定数値を記録し、
        initializerが最適化で消えた場合も再構成する。
        `test-debug-info-inline-expression`でC++20 legacy/verified両backend、i686/AMD64の
        nested block local origin/valueを検証。concrete lexical-block DIE/rangeとmutable localの
        location listは未対応
  - [x] safe scalar inlinerがcleanup・lifetime-sensitive objectのないnested lexical blockを
        arithmetic scalarに限定してsource順に展開し、address-taking／nested control flowを
        拒否してlocal bindingの同一性をDecl単位で保持。`test-optimize`でC17 i686/AMD64の
        O0呼出し保持、O1 call除去、および実行値を検証
  - [x] direct inline callの引数式をIR/codegenのdebug metadataまで保持し、
        signed/unsigned integral parameterに整数定数を渡す場合は、
        concrete `DW_TAG_formal_parameter`へabstract-origin参照とcall固有の
        `DW_AT_const_value`をsigned `sdata`／unsigned `udata`で出力。
        inline展開で整数型parameterへargument castを保持し、表現可能な
        signed変換とC規定のunsigned modulo変換にも対応。legacy/verifiedの
        i686/AMD64すべてで`test-debug-info`を実行し、signed値23、unsigned値
        `UINT64_MAX`、`short`→`int`、`-1`→`unsigned int`を検証。
        `test-optimize`ではruntime引数の変換結果も確認。非定数parameterと
        register/location listは未対応
  - [x] verified backendで最適化inlineのorigin／呼出し位置をIRからMIR・x86
        legalization・encoded PC rangeまで伝播し、relocation付きDWARF inline DIEを
        出力。C++ inline host/leafのabstract origin、call-site file/line/column、
        function内PC rangeをi686/AMD64 `-g -O2 -fverified-backend`で検証。nested
        `host -> middle -> leaf` call-siteのline/columnもfixture上の正確な位置と照合
  - [x] verified C++ inlineを`host -> middle -> leaf`の2段へ拡張し、host内の
        `DW_TAG_inlined_subroutine`親子構造、両call-siteのsource line/column、
        relocation先、child PC rangeのparent containmentをi686/AMD64
        `test-debug-info`で検証
  - [x] verified backendでqualified static member callのinline callee source
        columnを呼び出し式先頭と照合し、DWARF call-siteのline/columnをi686/AMD64
        `test-debug-info`で検証
  - [x] namespace-scope `constexpr` function-pointer変数が互換な関数を直接指す
        場合に限りcalleeを解決し、既存のpure-scalar・副作用・予算条件を通るcallを
        inline化。i686/AMD64 DWARFのcall-site line/columnとhost relocationを検証
  - [x] C/C++の`(*callbacks[N])(Args...)`配列declaratorを解析し、C17 callable
        arrayをi686/AMD64で実行検証。C++20ではnamespace-scope constexpr
        function-pointer arrayの定数添字を安全に解決し、既存のpure-scalar・副作用・
        予算条件を通るcallだけinline化。verified DWARF call-site (`89:12`) とhost
        relocationをi686/AMD64、結果実行をAMD64で検証
  - [x] 関数本体内で初期化された `constexpr` / `const` function pointerと、const要素を
        持つlocal function-pointer arrayについて、直接の関数初期化子と互換型を確認
        できるscalar callをinline化。i686/AMD64の最適化objectを生成し、AMD64でlocal
        function relocationを適用してpointerと定数添字配列の結果を検証
  - [x] namespace-scopeの非constexpr `const` function pointerとconst要素配列も、
        非dynamic initializer・直接の関数target・互換型・定数添字を確認して対象化。
        i686/AMD64 -O0/-O1 objectを生成し、AMD64では間接call比較と実行で検証。
        `test-optimize`は成功。compiler sourceは`8582d8c`から未変更で、同commitの後に
        `test-debug-info`と完全な`test-cxx`も成功済み
  - [x] automatic localのmutable function-pointer scalarと固定長arrayは、関数全体を
        調べて再代入・要素書込み・escapeがないこと、非volatile、直接の互換function
        initializerとscalar callee型の一致を確認できる場合にinline化。arrayは範囲内の
        整数定数添字、または全要素が同じ直接targetを持つside-effect-free可変添字に限定。
        i686/AMD64の-O1 indirect-call形状とAMD64実行を検証
  - [x] scalar local function pointerについて、初期化後のすべての直接代入が同一の
        関数targetで、address/reference escapeやgoto/switchによる初期化迂回がない
        場合も-O1 inline化。条件分岐内の同一target再代入を含め、i686/AMD64の
        indirect-call形状とAMD64実行を`test-optimize`で検証
  - [ ] mutable localの再代入・要素書込み、pointer/reference aliasによるescape、異なる
        targetへの再代入、targetを選びうる可変添字、より一般のaliased/indirect callの
        解析・最適化は未実装。
        calleeを静的に安全確定できない呼び出しは引き続き間接callとして保持
  - [x] 最適化でstorageが除去されたimmutable unsigned scalar localの値を
        `DW_AT_const_value`の`DW_FORM_udata`で保持し、符号付き負値からunsigned型への
        modulo変換と`UINT64_MAX`をi686/AMD64で検証。signed localには`sdata`を使い、
        unsigned値をsigned 64-bitへ切り詰めない。
  - [x] 最適化でstorageが除去されたimmutable `float`／`double` literal localの
        IEEE-754値を`DW_AT_const_value`の`DW_FORM_block1`で保持し、4/8-byteの
        exact bit patternと偽のframe locationがないことをi686/AMD64で検証。
        `test-debug-info`、`test-optimize`、`test-verified-backend`が成功。
  - [x] 最適化でstorageが除去されたimmutable C enum localのsigned値を
        `DW_AT_const_value`で保持し、暗黙の`int`互換underlying representationを
        確認したうえで、DWARF constant値とframe locationなしをi686/AMD64で検証。
        `test-debug-info`、`test-optimize`、`test-verified-backend`が成功。
  - [x] `test-debug-info`でlinked RINのABI v4、単一UNWIND section、
        `READ | UNWIND_DWARF_DEBUG_FRAME` flagsを両architectureで確認し、
        `test-format-validation`から同じdebug-frame形式のRINをrinvalidateへ
        渡して受理を検証。
  - [x] `-g`の有無でruntime `.unwind` FDE coverageを変えず、source locationを
        持たないsynthetic `__rcc_global_init`も含める。i686/AMD64のdebug／
        non-debug object relocation回帰とlinked debug RINのrinvalidate検証を
        `test-debug-info`／`test-format-validation`で確認。
  - [x] Static address constantを直接のglobal/static object identifierから
        nested non-bitfield memberとliteral array indexへ拡張し、元symbolと累積byte
        addendを維持する。`&object.values[1]`をaddend 8としてlegacy／verified
        optimized DWARFのi686/AMD64で検証し、native Windows `test-debug-info`が成功。
  - [x] 関数／lexical-block DIEの`DW_AT_decl_file`を32-bitで出力し、256件を
        超えるsource file tableでもindexを切り詰めない。260個の`#line` fileを
        生成する回帰でfunction DIEとnested lexical-block DIE双方のindex／
        line-table entry対応をi686/AMD64で検証し、native Windows
        `test-debug-info`全体が成功。
- [ ] Extend the existing bounded inline DIE and frame-relative location-list
        coverage beyond the implemented direct-body const scalar values,
        foldable signed/unsigned inline-parameter constants and supported
        integer conversions, and same-typed identifier arguments whose source
        storage is a defined static object or an addressable frame slot and
        whose callee body is a bounded side-effect-free return/if-else tree.
  - [x] Preserve runtime signed/unsigned integer arguments formed by bounded
        unary `-`/`~` and binary `+`, `-`, `*`, signed `/` and `%`, `&`, `|`,
        `^`, `<<`, and `>>` expression trees over same-typed constants and
        addressable frame locals.
        Emit a target-width-correct DWARF expression that loads and normalizes
        signed or unsigned locals, selects arithmetic/logical right shift by the
        result type, width-normalizes each operation, synthesizes signed
        remainder as `a - (a / b) * b` without relying on ambiguous `DW_OP_mod`
        semantics, and leaves the value on the DWARF stack. Decode each
        parameter location and cross-check its source frame slot and exact
        `DW_OP_div`/`DW_OP_neg`/`DW_OP_not` opcodes plus the full remainder
        expression for legacy and verified i686/AMD64 `-g -O1` objects in
        `test-debug-info`.
  - [x] Preserve unsigned `/` and `%` expressions when the operand width is
        narrower than the target address width, so zero-extension keeps each
        value within the signed range understood by generic DWARF stack
        operations. Verify `unsigned int` `/ 3u` and `% 5u` on AMD64 in legacy
        and verified `-g -O1` objects, including the 32-bit normalization,
        exact `DW_OP_div`/synthesized remainder operations, and source frame
        slot. Keep full-width unsigned operands out of signed `DW_OP_div`.
  - [x] Lower full-width unsigned `/` and `%` when the operand width equals
        the target address width (i686 `unsigned int`, AMD64
        `unsigned long long`) without passing full-range values to signed
        `DW_OP_div`. For low divisors, halve the dividend before signed
        division and reconstruct quotient/remainder; for high divisors,
        compare unsigned values by flipping the sign bit and use the resulting
        zero-or-one quotient. Evaluate the emitted DWARF expressions at
        32/64-bit boundaries with low and runtime high divisors in legacy and
        verified `-g -O1` objects through `test-debug-info-inline-expression`.
  - [x] Record i686 inline-call ranges for 64-bit expressions lowered through
        the EDX:EAX pair path. Preserve an unchanged `unsigned long long`
        inline parameter passed from an addressable caller frame slot as a
        frame-relative memory location instead of truncating it to a generic
        32-bit DWARF stack value. Verify the exact location and matching source
        frame slot in legacy and verified `-g -O1` objects.
  - [x] Preserve computed signed and unsigned i686 `unsigned long long`
        inline-parameter values for `&`, `|`, and `^` over two addressable
        frame identifiers. Emit the low/high 32-bit results as separate
        `DW_OP_piece` values and verify the exact source frame slots, operation,
        and evaluated result for legacy/verified i686/AMD64 `-g -O1` objects.
        The focused `test-debug-info-inline-expression` gate passes on POSIX
        and native Windows.
  - [x] Preserve signed and unsigned i686 64-bit inline-parameter results for
        `+`, `-`, and `*` over two addressable frame identifiers. Verify the
        source frame slots, low/high `DW_OP_piece` values, and boundary-value
        evaluation in legacy/verified C and C++20 `-g -O1` objects for both
        architectures with `test-debug-info-inline-expression`; the native
        Windows gate passes.
  - [x] Preserve the inline wide-multiply source locals' location lists and
      exact frame slots when they are declared two lexical blocks below the
      entry function. Verify the evaluated expression in legacy and verified
      i686/AMD64 `-g -O1` objects through `test-debug-info-inline-expression`.
  - [x] Compose runtime i686 64-bit left and logical-right shifts inside
      bounded two-operand inline-parameter addition trees. Split each result
      word across 0, 1, 31, 32, 33, and 63-bit counts, preserve the frame slots
      for the wide source, count, and addend, and evaluate boundary values in
      i686 legacy and verified `-g -O1` objects with
      `test-debug-info-inline-expression`.
  - [x] Recursively compose those runtime shifts through bounded i686 64-bit
      bitwise AND/OR/XOR inline-parameter trees. Preserve the wide source,
      count, and right-hand frame slots and evaluate each word for the same
      shift-count boundaries in legacy and verified `-g -O1` objects through
      `test-debug-info-inline-expression`.
  - [ ] Other unsigned operands wider than the target address width (notably
        i686 `unsigned long long`) in computed expressions beyond the bounded
        pairwise add/subtract/multiply/bitwise operations above, arbitrary
        nonconstant argument expressions, mutable parameters,
        optimized-away/register-only sources, nested lexical-block DIEs/ranges
        and mutable inline locals, tracking
        optimized variables across register and piece locations, declaration
        lifetimes across every optimized CFG edge, and CFI/unwind coverage for
        prologues and epilogues beyond current fixed-frame rules. General
        variable-location implementation remains incomplete.
- [ ] inline asm constraintの完全検証
  - [x] bounded i686/AMD64固定レジスタ制約、出力lvalue・scalar型、clobber、
        numeric `%N` placeholderの固定レジスタ展開、`%%` escape、および
        `mov source,destination`の実バイト生成を実装。名前付き／modifier／
        unsupported operandは明示診断し、未知制約の黙殺を禁止
  - [x] 固定レジスタoperand同士、operandとclobber、重複clobberの衝突を
        backendへ渡す前に診断し、既存の`=a`出力と`a`入力のtie相当だけを
        維持する両arch回帰を追加
  - [x] 汎用`=r`／`+r` output constraintをi686のECXとAMD64のR10へ
        決定的に割り当て、入力評価・asm本体・lvalueへの結果保存まで実命令で
        lowerする。C/C++両frontend、両archのencoding・execution回帰と、
        `r10` clobberを含む衝突診断を追加
  - [x] 整数定数式の`i`／`n` input constraintを意味解析で検証し、両archの
        `%N` placeholderを即値へ展開して`int $imm8`を実バイト生成する回帰と、
        非定数入力を拒否する診断を追加
  - [x] i686の汎用`r`／`X` input constraintをECXへ決定的に割り当て、重複・
        clobber衝突を診断して`mov`の実バイト生成まで両archで回帰検証
  - [x] GCC互換の空output `::` 構文、`Nd` port constraintの即値／DX選択、
        `%bN`／`%wN`／`%kN` register modifier、`inb`／`inw`／`inl`／
        `outb`／`outw`／`outl`の実エンコード、およびmodifier付き`mov`の
        byte/word/dword encodingをi686/AMD64のC/C++で検証
  - [x] `rcc/intrin.h`／`x86intrin.h` のCPUID、RDTSC/RDTSCP、MSR、control
        register、I/O、fenceを受理だけにせず実x86命令へエンコードし、
        i686/AMD64のC/C++ヘッダコンパイルと生成バイトを回帰検証。C++の
        intrinsic vector型Itanium name manglingも同時に実装
  - [x] Avoid redeclaring `__m128d` and `__m128i` in `emmintrin.h` after
        `xmmintrin.h` supplies them; pass `test-intrin-header` for C and C++20
        on i686 and AMD64.
  - [x] GCC `g` input/output constraintは有効なGPR alternativeを決定的に
        選び、`=`／`+` outputの保存、input評価、衝突検査を通してlowerする。
        Cでi686/AMD64 byte・生成検証とx64実行、C++でx64生成byteを検証し、
        `test-inline-asm-execute`をproduction `test-ci`へ追加
  - [x] Support GCC `%qN` as an AMD64 64-bit general-register name in C/C++ inline
        asm templates; reject it on i686 and reject applying `%qN` to immediate
        operands. Verify exact
        `mov %rbx, %rax` bytes in
        C/C++, execute the generated C function, and cover both diagnostics in
        `test-inline-asm-validation`.
  - [x] Support GCC `%cN` constant formatting in i686/AMD64 C/C++ inline asm:
        emit integer constants without the target immediate prefix, reject
        register operands with an explicit diagnostic, and verify exact `int`
        bytes plus both-target negative cases in the inline-asm gates.
  - [x] Support GCC `%nN` constant formatting in i686/AMD64 C/C++ inline asm,
        including INT64_MIN-safe sign inversion, exact negative-immediate bytes,
        and explicit diagnostics for register operands.
  - [x] Support bounded i686 input-only `m` operands for direct automatic frame
        variables and parameters; lower EBP-relative `movl` loads. Verify exact
        instruction bytes, reject global/indirect i686 operands, and include
        i686 execution in the POSIX inline-asm execution gate.
  - [x] Add i686 inline-asm `=m`/`+m` outputs and indirect `m` operands with
        conflict-free address-register selection and correct output lvalue
        handling. Verify exact i686 encodings for indirect loads/stores,
        read-write memory, multiple memory operands, and x87/MXCSR environment
        forms; compile the `math.h`/`fenv.h` include path in C17/C++20 on both
        targets; and execute fenv plus indirect memory read/write tests on
        i686 and AMD64.
  - [x] Add AMD64 `m`, `=m`, and `+m` lowering for direct frame objects and
        computed lvalue addresses, with address-register selection that avoids
        operand assignments and clobbers. Extend memory/register `mov` widths,
        `addl` immediate memory updates, and the x87 environment/control/status
        plus MXCSR memory forms used by `fenv.h`. Verify exact AMD64 bytes,
        execute indirect loads/stores/read-write and multiple-memory templates,
        compile `fenv.h` in C17/C++20, and reject non-lvalue memory inputs.
  - [x] Exercise the bounded memory input through the C++20 frontend on i686;
        compile the i686 fixture and verify its exact EBP-relative `movl` bytes
        alongside the C and AMD64 C++ encoding gates.
  - [x] Encode the bounded i686 register-indirect/displacement `mov` load/store
        and `lea` forms, including ESP SIB and EBP zero-displacement cases, plus
        register `xor`/`test`, indirect `jmp`, and `pushl`/`popl`. Check exact
        bytes, run the memory/branch/indirect-jump fixture on the POSIX i686
        host gate, preserve naked assembly-owned entry/exit sequences, and keep
        i686-only addressing from being misencoded on AMD64.
  - [x] Resolve GNU numeric local labels and nearest `nf`/`nb` references within
        each inline-asm template. Exercise forward conditional/unconditional
        branches at runtime on i686 and compile a backward-reference regression.
  - [x] Preserve a generic inline-asm output value while computing its lvalue
        address; verify `=r` ECX assignment on i686 and AMD64 execution gates.
  - [x] Assign generic `=&r` outputs to a physical GPR distinct from all
        backend-assigned generic/fixed inputs, outputs, and clobbers. Verify
        exact i686/AMD64 C and AMD64 C++ encodings, execute both target-width C
        regressions, and diagnose impossible fixed-register overlap.
  - [x] Encode AT&T immediate-to-GPR `mov` for i686 8/16/32-bit and AMD64
        8/16/32/64-bit destinations, with signed/unsigned destination-width
        range checks. Verify exact C/C++ bytes, execute every width (including
        the AMD64 64-bit value), and reject an out-of-range byte immediate.
- [x] driver modeでのFPU/SIMD禁止検査

## Aquamarine shader frontend

- [x] `.aq` sourceをRinShader `RSH1`へloweringするnative `aqc`をRinCompilerへ統合
  - [x] vertex／fragment／compute、typed IO/resource、scalar Int32／Float32／bool
  - [x] immutable local、演算／比較／変換、forward if/else、stage builtin、storage／sampling／discard
  - [x] source／identifier／nesting／register／instruction上限、definite output、resource kind検査
  - [x] RinGPU共有validatorによる生成RSH1の再検査とCLI／host corpus
- [ ] vector／matrix、interpolation、bounded loop、atomics／barrier、derivative／storage image
- [ ] optimization、source map、module linker、SPIR-V／DXIL import、backend code generation

## 6. bootstrap / quality gates

- [x] `#pragma pack`/`offsetof`を両arch layoutへ接続し、archive/linker、v3 packager、`.ro v2` emitterを再現bootstrap対象へ追加
- [x] preprocessor、static assert、x86_64実行ABIのhost回帰試験
- [x] SDK v1を両archの`.ra/.rll`へpackageする統合経路
- [x] production validatorによる署名付き成果物検査
- [ ] frontend/sema/IR/pass/backend単体試験の体系化
- [x] C17/C++20 aggregate、IR/MIR、verified backend、optimizerをhost CIでgate
- [x] Add `test-aqc` to the GCC/Clang production regression matrix and make its
      production shader fixtures resolvable from the nested RCC checkout; the
      native Windows target passed for both vertex and fragment shaders.
- [x] C++ `new[]`の基本／constructor initializer、両target object、AMD64 host
      execution、および不正なparenthesized initializer診断を検証する
      `test-cxx-new-array`を`CXX_REGRESSION_TARGETS`へ追加し、`test-cxx`経由で
      production `test-ci`に含める。既存focused targetはWindows hostでpass。
- [x] CI workflowでfull `test-ci` production gateをGCC/Clang双方に設定し、
      C17/C++20、IR/MIR、verified backend、optimizer、ABI、image、bootstrapを
      matrix実行。Makefileの環境変数／command-line `CC`保持と、built-in `cc`
      のみの場合のGCC defaultを回帰検証
- [x] GCC full `test-ci` production gateでC17/C++20、IR/MIR、verified backend、
      optimizer、ABI、image、bootstrap全経路の成功を確認し記録する
  - [x] GCC: `make -j1 OBJDIR=build/wsl-gcc/obj
        BINDIR=build/wsl-gcc/bin TEST_OUT=build/wsl-gcc/tests CC=gcc test-ci`
        passed on the Linux/WSL host, including both target architectures.
- [x] Clang full `test-ci` production gateを実行し、同じ全経路の成功を記録する
  - [x] Native WindowsでMSYS2 Clang 18.1.4を使い、分離した
        `OBJDIR=obj/clang`でRCC++をビルドして
        `test-cxx-function-template-overloads`を実行。i686/AMD64出力、AMD64
        実行、曖昧性・交差変換negative、GCC oracleを確認。これはfocused
        host/compiler検証であり、full `test-ci`完了の代用ではない。
  - [x] Native WindowsでClang 18.1.4をhost linker/test compilerとして
        `test-compiler-builtins`を実行。Clang LLDのi386 PE entry解決では
        RCCの`main`シンボルを`_main`へ対応させる必要があるため、テスト用
        COFF objectだけをrenameしてC/C++ builtin、MMX、SSEのi686実行を維持。
        x64実行、intrinsic object生成、negative診断も通過し、GCC側の同じ
        targetも回帰確認した。これは既存RCC実行ファイルを使ったfocused
        host検証であり、Clang full `test-ci`完了の代用ではない。
  - [x] Native WindowsでClang 18.1.4を指定したfull `test-ci`を開始し、RCCを
        Clangでビルド。C17とC++ regressionは`test-cxx-alias-templates`まで進み、
        同targetのi686側は通過したがAMD64 assembly生成時に
        `cannot write assembly output`で停止。
        その時点でE:の空き容量は0 bytesであり、容量不足と確認した。これは
        full gateの成功ではない。GCCへ戻した後、`test-debug-info`、
        `test-cxx-function-templates`、`test-cxx-class-template-dependent-nested-type`
        は再実行して通過。
  - [x] BFDの`pe-i386`/`i386:x86-64`とLLVMの`coff-i386`/`x86_64`という
        COFF machine表記差をWindowsの`rcc_grep.ps1`で同一architectureとして扱う。
        Clang 18.1.4でfull `test-ci`を再実行し、C17/C++20、IR/MIR、verified
        backend、optimizer、ABI、image validation、golden/fuzz、linker等の
        Windows実行可能なtargetが通過した。最初のWSL-only bootstrap target
        `test-bootstrap-core`で`Wsl/Service/E_ACCESSDENIED`となり、この結果は
        full `test-ci`の成功ではない。stage1実行とstage2再現は未実施。
  - [x] 2026-10-13にMSYS2 Clang 18.1.4で同一source revisionの
        `test-ci`全体を再実行し、C17/C++20、AQC、IR/MIR、verified backend、
        optimizer、ABI、RIN/RLL/DRV format、golden、determinism/property/fuzz、
        linker、stage0 core、stage1 link/execution、stage2再現までexit 0。
        WSL Ubuntu-24.04を含む全bootstrap段も完了した。
- [x] ASan/UBSan regression gateもGCC/Clang双方の独立matrix jobで実行し、
      各jobが要求したhost compilerを実際に選択していることを検証
- [x] CI regression gateでC/C++ global initializer/finalizerのhost実行、
      `.init_array`/`.fini_array`伝播、RIN/RLL/DRV/RLD image validationを常時gate
- [ ] clang/gcc互換の全golden `.ro/.ra/.rin/.rll/.drv` corpusとfuzz corpus
  - [x] bounded C17/C++20 property corpusで両archの再生成一致と不正入力の
        明示`error:`診断をCI gateする
  - [x] RCC単体checkoutから実行できるbounded C17/C++20 golden manifestで、
        i686/AMD64の`.ro`、unsigned-v3`.rin`、`.rll`、`.drv`を2回再生成し、
        SHA-256をCIで固定検証する
  - [x] C17 atomic languageとbounded C++20 `typeid`のgolden caseを追加し、
        両archの`.ro/.rin/.rll/.drv`を反復生成してhash一致を確認、v3画像は
        `rinvalidate`にも通す
  - [x] RCC単体checkoutから実行できる決定的parser/compiler fuzz gateで、
        C17/C++20の有効変異と決定的に生成した式・aggregate・template・
        分岐・loop構文を両archで再現コンパイルし、無効変異のsignal／timeout／
        空診断／誤った成果物を拒否する。24 generated syntax trees、80 valid
        source/target pairs、16 invalid casesをnative Windowsで検証
  - [x] sanitizer CIに同じfuzz corpusをASan/UBSan instrumented rcc/rcc++で
        実行するstepを追加し、GCC/Clang matrix双方へ適用
- [x] host stage0 -> RCC stage1 -> RCC stage2再現build。i686/x86_64の全compiler
      sourceをstage1で二重生成して一致比較し、両arch stage1 imageのlink／実行、
      stage2 objectとimageの完全一致まで確認
  - [x] stage0による閉じたfrontend/sema/backend subset `.ro`の両arch再現生成
  - [x] build manifest、host process shim、rcc/rcc++/rld/rar entry pointまでの再現object生成
  - [x] `rincrt.rll` typed import付きrcc stage1 RIN v3 imageの両arch再現link
  - [x] host RIN v3 runnerで両arch stage1を実行し、probe `.ro v2`のstage0一致を検証
  - [x] stage1によるcompiler全translation unitのstage2再生成とimage一致
- [ ] RinOS i686セルフホスト
- [ ] RinOS x86_64セルフホスト

## Host test recipe audit (2026-10-04)

- [x] Make the C++ overload regression recipe use the shared host-shell
      helpers for directory creation, expected failures, and fixed diagnostic
      matching; the complete target passes with native Windows `cmd.exe`.
- [x] Keep host-specific object and GCC dependency files in separate ignored
      `obj/windows` and `obj/posix` outputs so an existing native Windows
      `obj/*.d` file with drive-letter prerequisites cannot stop WSL before a
      test recipe starts.
- [x] Make C++ language-linkage symbol checks use the shared host text helper,
      and run namespace parser-recovery diagnostics through a bounded Windows
      PowerShell process monitor as well as the POSIX timeout path.
- [x] Make the native Windows `cmd.exe` C17 preprocessing and language-boundary
      gates use the shared directory/expected-failure helpers, native tool names,
      and a committed PowerShell text matcher; i686 runtime execution remains
      explicitly dependent on a 32-bit host runtime or RinOS/WSL runner.
- [x] Recover fixed-string patterns containing embedded double quotes from the
      original Windows command line when `cmd.exe` splits single-quoted arguments;
      `test-preprocessor-operators` passes with native Windows `cmd.exe`.
- [x] Emit file-scope data/BSS variable DIEs with source locations, type
      references, external-linkage flags, and `DW_OP_addr` relocations for
      external and internal-linkage symbols; verify x86/x64 `.ro` objects and
      linked `.rin` output in `test-debug-info`.
- [x] Emit `DW_AT_linkage_name` for file-scope variable DIEs so C++ source
      names retain their mangled ABI symbol identity in debug information.
- [x] Emit static-local variable DIEs as subprogram children with absolute
      symbol relocations, source locations, and collected types; verify the
      i686/AMD64 object and linked-image debug-info path.
- [x] Emit C/C++ global and C static-local TLS variable locations with
      `DW_OP_const4s`/`DW_OP_form_tls_address` and `TLSOFF32S` relocations;
      collect TLS symbols and verify C, C++, and verified-backend objects for
      i686/AMD64 in `test-debug-info`.
- [x] Make the native Windows verified-backend bridge use VirtualAlloc/
      VirtualProtect and SysV-ABI function-pointer adapters, so it parses both
      target objects and executes native x64 without requiring MinGW's absent
      32-bit CRT libraries.
- [x] Preserve labels nested below an already-terminated parent control
      statement by lowering explicit label entries instead of leaving an
      unreachable or unterminated SSA block; cover the optimized nested-goto
      regression in the dual-architecture optimizer gate.
- [x] Make native Windows `test-optimize` use portable byte/count helpers,
      host-native object verifiers, and explicit i686 inspect-only coverage;
      x64 cleanup execution remains fully enabled, while i686 execution stays
      dependent on a 32-bit host runtime or an available RinOS/WSL runner.
- [x] Make native Windows compiler-builtin, C++ builtin, MMX, and SSE runtime
      fixtures execute their i686 assembly through a freestanding `main`
      entry when MinGW's 32-bit CRT is absent, and route all expected-failure
      diagnostics through the shell-neutral helper so `test-compiler-builtins`
      passes under `cmd.exe` without skipping x86 execution.
- [x] Cover the GCC-compatible `__builtin_parity`, `__builtin_parityl`,
      `__builtin_parityll`, `__builtin_ffs`, `__builtin_ffsl`, and
      `__builtin_ffsll` families in C and C++: semantic width diagnostics,
      SWAR/direct x86/x86-64 code generation, verified-backend C/C++ fixtures
      for the supported typed-SSA widths, zero/one-based result coverage, and
      negative checks are gated by `test-compiler-builtins` and
      `test-verified-builtins`.
- [x] Make the native Windows integer-literal gate execute real i686/x86_64
      RCC output through freestanding `main` entries, while retaining `.ro`
      generation and shell-neutral negative diagnostic checks.
- [x] Make the native Windows integer-promotion gate execute real i686/x86_64
      RCC output through freestanding `main` entries, while retaining `.ro`
      generation and shell-neutral negative diagnostic checks.
- [x] Make the native Windows integer-conversion gate execute real i686/x86_64
      RCC output through freestanding `main` entries, while retaining `.ro`
      generation and the POSIX object-loader path.
- [x] Make the native Windows function-call contract gate execute real
      i686/x86_64 RCC output through freestanding `main` entries, while
      retaining `.ro` generation, invalid-call diagnostics, and the POSIX
      object-loader path.
- [x] Make the native Windows varargs ABI gate execute real i686/x86_64 RCC
      output through freestanding `main` entries, while retaining `.ro`
      generation, invalid-varargs diagnostics, and the POSIX object-loader
      path; route the invalid case through the shared `EXPECT_FAILURE` and
      text-matcher helpers so native PowerShell does not wrap diagnostics.
- [x] Make the native Windows inline-asm constraint gate route both expected
      failures through the shared `EXPECT_FAILURE` helper without a multiline
      `cmd.exe if` wrapper, so every diagnostic is executed and matched on
      both i686 and x86_64.
- [x] Make the native Windows signing gate create its deliberate
      semicolon-and-space signer path through the shell-neutral `MKDIR_P`
      contract, avoiding nested quotes while still exercising argv-safe
      signer invocation.
- [x] Make the native Windows signing gate use explicit PowerShell file-copy,
      absence, byte-compare, temporary-file, and concurrent-process helpers;
      the full atomic-publication contract is now executable under `cmd.exe`.
- [x] Make Windows RCC/RLD signer invocation preserve every argv element,
      including signer paths containing spaces, quotes, and semicolons, by
      constructing a Windows-compatible command line and waiting for the
      child process through `CreateProcessA`.
- [x] Make the native Windows scalar-comparison and scalar-truth gate execute
      real i686/x86_64 RCC output through freestanding `main` entries, while
      retaining `.ro` generation, invalid-comparison diagnostics, and the
      POSIX object-loader path.
- [x] Make the native Windows aggregate-copy ABI gate execute real
      i686/x86_64 RCC output through freestanding `main` entries, while
      retaining `.ro` generation, anonymous-member diagnostics, and the POSIX
      object-loader path.
- [x] Make the native Windows aggregate-return ABI gate execute real
      i686/x86_64 RCC output through freestanding `main` entries, while
      retaining `.ro` generation and the POSIX object-loader path.
- [x] Make the native Windows packed-aggregate ABI gate execute real
      i686/x86_64 RCC output through freestanding `main` entries, while
      retaining `.ro` generation and the POSIX object-loader path.
- [x] Make the native Windows automatic compound-literal gate execute real
      i686/x86_64 RCC output through freestanding `main` entries, while
      retaining `.ro` generation, incomplete-type diagnostics, and the POSIX
      object-loader path.
- [x] Make the native Windows flexible-array-member gate execute real
      i686/x86_64 RCC output through freestanding `main` entries, while
      retaining `.ro` generation, invalid-member diagnostics, and the POSIX
      object-loader path.
- [x] Make the native Windows floating static/TLS initializer gate inspect
      generated assembly through the shared file-based text helper, preserving
      both positive and negative IEEE-754 byte-pattern checks.
- [x] Make the native Windows i686 floating-runtime gate execute RCC output
      through a freestanding `main` without WSL, while retaining the Linux
      syscall start path and checking the floating/ABI calculation result.
- [x] Make the native Windows numeric-literal gate execute its own C17
      fixture main through freestanding i686/x86_64 outputs, while retaining
      POSIX execution and invalid universal-character diagnostics.
- [x] Make the native Windows VLA runtime gate execute real i686/x86_64 RCC
      output through freestanding `main` entries without WSL, retaining the
      POSIX syscall-start path and the full VLA runtime result checks.
- [x] Make native Windows VLA declaration diagnostics use the shared
      shell-neutral expected-failure helper, retaining per-target logs and
      diagnostic-text assertions.
- [x] Make native Windows VLA goto/array-parameter semantic diagnostics use
      the shared expected-failure and fixed-string matcher helpers, preserving
      complete native compiler error lines.
- [x] Make native Windows preprocessing, old-style C, parenthesized C++ array-
      new, and C parser-recovery negative gates preserve complete diagnostics
      through the shared expected-failure/text-matcher helpers or separate
      stdout/stderr files; keep the parser-recovery process bounded.
- [x] Make native Windows VLA declarator-variant and static-local runtime
      gates execute generated i686/x86_64 code through freestanding `main`
      entries without WSL, while retaining RIN emission and validation.
- [x] Make native Windows IR/MIR host fixtures use the available CRT while
      retaining both explicit target-policy checks and a dedicated i686
      legal-IR encoder/object inspection path; x64 native execution remains
      enabled and no target is silently omitted.
- [x] Make native Windows TLS negative checks use the shared shell-neutral
      expected-failure helper, retaining RIN/RLL generation, RLD linking, and
      invalid DRV diagnostics.
- [x] Execute native Windows x86_64 atomic-builtin output through VirtualAlloc,
      Win32 threads, and an explicit SysV-ABI function-pointer adapter, while
      retaining i686 object inspection and negative diagnostics.
- [x] Execute native Windows x86_64 `_Atomic` language output through
      VirtualAlloc and a SysV-ABI adapter, while validating i686 `.ro` layout,
      required symbols, and both-architecture negative diagnostics.
- [x] Make the native Windows i686 wide-scalar ABI gate validate `.text`/
      `.data`, ABS32 relocations, global storage, and all exported ABI symbols
      through an explicit inspect mode, while retaining native i686 execution
      on hosts that provide a 32-bit runtime.
- [x] Implement GCC-compatible `__atomic_always_lock_free` and
      `__atomic_is_lock_free` queries for 1/2/4/8-byte lock-free widths on
      i686/AMD64, preserving ignored-pointer side effects, validating C/C++
      argument types and constant requirements, and executing the generated
      queries through the atomic-builtin gate for both targets.
- [x] Preserve C-compatible `signed`/`unsigned` shorthand declarators in the
      shared C++ declaration parser instead of treating their identifiers as
      unknown type names; verify the fix through the dual-architecture C++20
      atomic fixture.

## Optimizer continuation (2026-10-09, C++ inline definition identity)

- [x] Resolve a deferred inline body using the declaration's ABI/link symbol
  in addition to source name, function type, and language linkage. This keeps
  a same-name/same-type global `static` function from being substituted for
  an anonymous-namespace function. The regression failed before the fix
  (returning 102 instead of 202), then passed with `test-optimize` on both
  target object formats and native AMD64 SysV execution.
- [ ] Broader call-graph/cost modeling, recursive and aggregate/exception
  inlining, and general loop transformations remain open.

## Native Windows bootstrap verification (2026-10-10)

- [x] Run the current GCC `test-ci` host gates with
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j2 test-ci`;
  C17/C++20, IR/MIR, verified backend, optimizer, ABI, inline-asm, RIN/RLL/DRV
  format/golden, deterministic fuzz/property, and linker/image checks pass.
- [x] Complete the full stage0/stage1/stage2 bootstrap through Ubuntu-24.04 WSL.
  The current `make SHELL=cmd.exe test-ci` run completed successfully on
  2026-10-12, including both target architectures; the earlier WSL startup
  failures are superseded by this verified run.
- [x] Reproduce the `scripts/bootstrap_gate.sh` core phase with the native
  `rcc-ci.exe`: compile all 38 `CORE_SOURCES` twice for i686 and x86_64 and
  verify all 76 source/target object pairs are byte-identical.
- [x] Reproduce the stage1 link phase with native `rld-ci.exe`: link the 33
  listed RCC objects with all 77 runtime imports twice per target and verify
  identical unsigned-v3 RIN images for i686 and x86_64.
- [x] Run the focused C++20 dependent nested-type target with the Windows
  `SHELL=cmd.exe` recipe: freestanding i686 execution, PE-i386 object check,
  and AMD64 host-adapter execution all pass.

The full current-working-tree `test-ci` run now covers those focused checks,
stage1 execution, and stage2 bootstrap; RinOS-native i686/x86_64 self-hosting
remains a separate open item.

## C++ regression follow-up (2026-10-10, dependent using and lookup diagnostics)

- [x] Parse the dependent class-template specialization in local-class
  `using Base<T>::Base` / `using Base<T>::member` before consuming `::member`;
  do not route the suffix through nested-type lookup. The existing
  `test-cxx-template-local-classes` fixture now compiles for i686/AMD64 and its
  generated x64 executable passes.
- [x] Keep ambiguous inherited method calls on the member-overload lookup path
  so the diagnostic reports the ambiguity directly instead of cascading into
  “no member” and “called object is not a function”. The transitive dependent
  base negative gate verifies exactly two errors and no `.ro` on both targets;
  GCC C++20 rejects the same fixture.
- [x] Run the complete native-Windows `test-cxx` aggregate on RCC `main` after
  these fixes. The full target exits successfully, including local-class
  templates, transitive dependent-base lookup, concepts/requires forms, and
  both-target invalid-source checks.
- [ ] Complete local-class template constructor/member ABI corner cases,
  full dependent lookup/overload resolution, and the broader C++20 conformance
  items above; this regression pass is not a C++20-completion claim.

## C++ regression follow-up (2026-10-10, function-template conversion ranking)

- [x] Compare viable function-template candidates' per-argument implicit
  conversion sequences before using template-pattern specificity as a
  tie-break. The `T` versus `T*` overload fixture selects the generic `T`
  candidate when its `short`-to-`int` promotion is better than the competing
  `short`-to-`long` conversion, despite the latter candidate's more-specialized
  pointer pattern. RCC-generated i686 and AMD64 executables and native GCC
  C++20 all pass; the GCC check is part of Windows and POSIX overload targets.
- [ ] Complete standard function-template partial ordering and conversion
  ranking, including reference/cv adjustments, packs, constraints, and less
  common conversion-sequence ties. This focused fix is not full conformance.

## C++ regression follow-up (2026-10-10, crossed function-template conversions)

- [x] Preserve crossed per-argument conversion vectors as incomparable and do
  not let the pattern-specificity fallback select a winner. The regression's
  candidates require `[2, 1, 0]` and `[0, 2, 0]` conversion ranks respectively;
  GCC and RCC both diagnose ambiguity, and RCC emits no `.ro` for i686/AMD64.
  The focused overload target includes the GCC reference check, and the full
  native-Windows `test-cxx` aggregate passes.
- [ ] Extend conversion-sequence comparison beyond the current rank vector to
  identity/subsequence, qualification, reference binding, derived-to-base,
  user-defined conversion, and full template partial-ordering interactions.

## C++ regression follow-up (2026-10-10, exact-match qualification ranking)

- [x] Distinguish identity from cv-qualification conversions in viable
  function-template candidate vectors, while retaining the existing coarse
  conversion ranks used by class-template deduction. Reject unsafe nested
  pointer qualification additions. The crossed identity/qualification versus
  promotion/conversion fixture diagnoses ambiguity on i686 and AMD64; GCC
  C++20 agrees, and RCC emits no `.ro`. The full native-Windows `test-cxx`
  aggregate passes.
- [x] Rank array-to-pointer decay as an exact-match transformation rather
  than a promotion, preserve the additional qualification step for
  `T[N]` -> `const T*`, and classify decay followed by pointer-to-void as a
  Conversion-rank sequence. The GCC-confirmed crossed regression is ambiguous
  on both RCC targets, produces no `.ro`, and passes with the full
  native-Windows `test-cxx` aggregate.
- [ ] Complete standard conversion-sequence ordering, including proper
  subsequences, reference-binding refinements, derived-to-base and
  user-defined conversions, plus their interactions with packs, constraints,
  and template partial ordering. This focused qualification fix is not full
  overload-resolution conformance.

## C++ regression follow-up (2026-10-10, function-template user conversions)

- [x] Reuse semantic-analysis conversion viability when ranking function-
  template candidates with fixed parameters, including public implicit
  conversion operators. A `Meter::operator int()` argument now makes the
  template viable, and the `int` result overload wins over a trailing
  `int`-to-`long` conversion. The target checks i686/AMD64 generation,
  verified-backend objects on both targets, AMD64 execution, and native GCC
  C++20 execution with warnings-as-errors.
- [ ] Complete standard conversion-sequence ordering, including proper
  subsequences, reference-binding refinements, derived-to-base conversions,
  UDC ties involving conversion-function selection, packs, constraints, and
  full function-template partial ordering. This fix is bounded, not full
  overload-resolution conformance.

## C++ regression follow-up (2026-10-10, conversion-function best candidate)

- [x] Continue conversion-function candidate ranking after a tie so a later
  better standard conversion can replace the tied candidates, and carry the
  selected conversion function into call resolution instead of looking up the
  shared internal name again. `RankedMeter` declares tied `float`/`double`
  conversion candidates before the exact `int` conversion; the focused target
  passes i686 and AMD64 generation/execution, verified-backend object checks,
  and native GCC C++20 `-Werror` execution.
- [ ] Complete standard conversion-sequence ordering, including promotion
  distinctions, subsequences, reference binding, derived-to-base conversions,
  conversion-function ties, packs, constraints, and full partial ordering.
  This fix remains bounded and does not claim general conformance.

## C++ regression follow-up (2026-10-10, standard promotion after conversion)

- [x] Rank integral promotions and `float`-to-`double` promotion ahead of
  ordinary arithmetic conversions after an implicit conversion function.
  `PromotionMeter` proves `operator short()` followed by `short`-to-`int`
  promotion wins over `operator long()` followed by `long`-to-`int`
  conversion, and `FloatingPromotionMeter` verifies `float`-to-`double`
  promotion beats `int`-to-`double` conversion. GCC C++20 and
  `test-cxx-conversion-operator` pass for both targets, runtime checks, and
  verified-backend object generation; the full native-Windows `test-cxx`
  aggregate also passes after the ranking change.
- [ ] Continue implementing conversion-sequence subsequences, reference
  binding refinements, derived-to-base ordering, and full template candidate
  interactions; promotion support here is bounded.

## C++ regression follow-up (2026-10-10, qualification-subset ranking)

- [x] Retain instantiated parameter targets with each function-template
  conversion rank and, for same-rank non-member pointer qualification
  conversions, prefer a target whose added cv qualifiers are a strict subset
  of the competing target. `int*` selects `const T*` over `const volatile T*`;
  incomparable `const T*` and `volatile T*` candidates remain ambiguous.
  GCC C++20 agrees; i686/AMD64 diagnostics and output, the focused overload
  target, and the full native-Windows `test-cxx` aggregate pass. No diagnostic
  suppression, stub, or fail-close path was added.
- [ ] Continue proper-subsequence ranking, reference-binding refinements,
  derived-to-base ordering, user-defined conversion ties, and complete
  function-template partial ordering.

## C++ regression follow-up (2026-10-10, reference-binding tie-break)

- [x] For equal-rank standard conversion sequences that both bind references,
  prefer a viable rvalue-reference binding to an rvalue over an lvalue-reference
  binding in both function-template and ordinary non-template overload
  resolution. `const T&` remains selected for an lvalue, while `const T&&` wins
  for prvalues and explicit xvalues. GCC C++20 agrees; i686/AMD64 generation,
  AMD64 execution, GCC `-Werror` execution, the focused overload target, and
  full native-Windows `test-cxx` pass.
- [x] Apply the same reference tie-break when both candidates use the same
  user-defined conversion function returning a class prvalue:
  `ReferenceConversionSource{}` selects `ReferenceConversionResult&&` over
  `const ReferenceConversionResult&` (`143` rather than `243`). The
  `test-cxx-conversion-operator` fixture and full native-Windows `test-cxx`
  aggregate cover the behavior.
- [x] Parse target-specific `using Base::operator T` declarations and rank a
      reintroduced base conversion against a same-target derived conversion.
      Compare result-conversion rank when only the base candidate needs a
      derived-to-base implicit-object conversion, then prefer the exact derived
      object binding on a tie. GCC C++20 agrees for `int`/`long` result choices;
      `test-cxx-conversion-operator` passes i686/AMD64 generation and RCC/GCC
      host execution.
- [ ] Ranking between distinct user-defined conversion sequences and the full
      set of implicit-object conversion relationships remain open.
- [x] Do not let a reference wrapper alone make a function-template pattern
  more specialized than its by-value counterpart when the non-pack scalar,
  class, or one-level unqualified-pointee pointer parameter lists become
  equivalent after reference adjustment. Top-level pointer cv is allowed on
  the reference candidate, while pointee cv is not. GCC and RCC both diagnose
  `T` versus `const T&`, `T&`, and `T&&`, plus `T*` versus `T*&`,
  `T* const&`, and `T* volatile&`, as ambiguous for the tested calls;
  and `T*&&` and `T* const&&` are ambiguous for xvalue pointer arguments.
  i686/AMD64 negative checks are in the focused overload target. Existing
  `T&&` versus `const T&&` cv ranking still passes. Member pointers, nested
  pointers, and cv-qualified pointees are intentionally not covered by this
  bounded adjustment rule.
- [ ] Continue proper-subsequence ranking, derived-to-base ordering,
  user-defined conversion ties, and complete function-template partial
  ordering.

## C++ regression follow-up (2026-10-10, pointer-to-void cv ranking)

- [x] Compare qualification subsets when a pointer-to-void standard conversion
  has overall Conversion rank: `int*` selects `void*` over `const void*`, while
  incomparable `const void*` and `volatile void*` targets remain ambiguous.
  GCC C++20 agrees; the focused test verifies i686/AMD64 generation, AMD64
  execution, exact RCC diagnostic counts, and GCC rejection. The full native-
  Windows `test-cxx` aggregate passed with the positive ranking case.
- [ ] Continue general proper-subsequence ranking, reference-binding
  refinements, derived-to-base ordering, user-defined conversion ties, and full
  function-template partial ordering; this is a bounded pointer-conversion
  rule, not complete standard conversion-sequence conformance.

## C++ regression follow-up (2026-10-10, pointer-to-bool ranking coverage)

- [x] Add a function-template regression where an `int*` argument can convert
  to either `bool` or `void*`; the non-bool pointer conversion must win at the
  same standard-conversion rank. GCC C++20 and RCC select `void*`; the focused
  target checks i686/AMD64 generation, AMD64 execution, and GCC `-Werror`.
  The compiler already implemented the expected result; this closes the
  missing regression-coverage item without changing compiler behavior.
- [ ] Continue proper-subsequence ranking, derived-to-base ordering,
  user-defined conversion ties, and complete function-template partial
  ordering.

## C++ regression follow-up (2026-10-10, derived-to-base pointer ranking)

- [x] Keep fixed-parameter derived-to-base pointer conversions viable while
  ranking function-template candidates. At equal Conversion rank, prefer
  `Leaf*` to `Middle*` over `Leaf*` to `Root*`, and a base-pointer conversion
  over conversion to `void*`; unrelated sibling-base conversions remain
  ambiguous. GCC C++20 agrees. The focused gate checks i686/AMD64 generation,
  AMD64 execution, exact RCC diagnostics, and GCC positive/negative cases; the
  full native-Windows `test-cxx` aggregate passes. No stub or fail-close path
  was added.
- [ ] Continue standard conversion ordering for pointer-to-member conversions,
  reference interactions, and general proper subsequences. This does not
  complete C++20 overload-resolution conformance.

## C++ regression follow-up (2026-10-18, class-by-value conversion ranking)

- [x] Verify that equal-rank derived-to-base conversions to class values prefer
  the nearer base in both ordinary and function-template overload sets. The
  `Leaf` argument selects `Middle` over `Root`; GCC C++20 agrees, both RCC
  target objects compile, and the native AMD64 RCC/GCC executions pass in
  `test-cxx-function-template-overloads`.
- [ ] Complete standard-conversion proper-subsequence ordering and its
  interactions with reference binding, user-defined conversions, constraints,
  packs, and full function-template partial ordering.

## C++ regression follow-up (2026-10-10, derived-to-base reference ranking)

- [x] Compare equal-rank derived-to-base conversions to class references by
  referred-to class, in both ordinary and function-template overload sets.
  `Leaf&` now selects `Middle&` over `Root&`; incomparable/unrelated targets
  retain their existing behavior. Keep this tie-break limited to matching
  reference/value forms and cv-qualified reference targets. The focused target
  passes i686/AMD64 generation, AMD64 runtime, existing ambiguity diagnostics,
  and GCC C++20 `-Werror` execution.
- [x] Add cv/value-category reference regressions for mutable and const
  lvalues, prvalues, and const xvalues; GCC C++20 and RCC agree in the focused
  overload target.
- [ ] Continue proper-subsequence comparisons, pointer-to-member conversions,
  user-defined conversion ties, constraints, parameter packs, and full
  function-template partial ordering; these bounded ranking rules do not
  establish complete C++20 overload-resolution conformance.

## C++ regression follow-up (2026-10-10, member-pointer owner ranking)

- [x] Rank viable data-member-pointer conversions from a base owner to related
  derived owners: `Root::*` selects `Middle::*` over `Leaf::*`, in ordinary and
  function-template overloads. Delegate member-pointer viability to semantic
  analysis instead of treating compatible member types as ordinary pointer
  conversions. GCC C++20 agrees; the focused target passes i686/AMD64
  generation, AMD64 execution, and both-architecture ambiguity diagnostics for
  unrelated owners, with no invalid `.ro` output.
- [x] Extend equal-rank member-pointer ranking for same-owner data-member
  targets whose member types differ only by a permitted cv qualification:
  `int Root::*` prefers `int Middle::*` to `const int Middle::*`, while
  `const` versus `volatile` remains ambiguous. Also rank same-signature
  member-function-pointer conversions across related owners, preferring
  `Middle::*` to `Leaf::*` for a `Root::*` source, in ordinary and function-
  template overloads. GCC C++20 agrees; the overload target passes both
  architectures, generated AMD64 execution, and positive/negative GCC checks.
  `test-cxx-member-pointer-data` and `test-cxx-member-pointer-functions` also
  pass for both architectures, including verified-SSA checks.
- [x] Rank member-function-pointer conversions across related owners when the
  target exception specifications also differ. GCC selects the nearer
  destination owner both when that target preserves `noexcept` and when the
  farther owner does; update ordinary and function-template candidate ranking
  to retain this owner ordering. The focused target passes i686/AMD64
  generation and AMD64 execution, with GCC C++20 `-Werror` syntax checks.
- [x] Re-run full native-Windows MinGW GCC `test-cxx` after the
  member-function-pointer owner/exception-spec ranking fix; the full target
  completed successfully, including both target architectures and generated
  AMD64 execution.
- [x] Route class-valued arguments targeting member pointers through public
  conversion-function lookup before rejecting direct member-pointer
  conversions. Cover data-member conversion results, base-to-derived owner
  adjustment for value arguments, differing member-function parameter
  signatures, and exact `noexcept` versus throwing targets. GCC C++20 agrees;
  the focused conversion-operator target passes i686/AMD64 generation and the
  generated AMD64 plus GCC-host executions.
- [x] Compare trailing member-type qualification sequences even when the
  general compatibility predicate gives both conversion-function targets the
  same rank. A GCC-confirmed function-template regression verifies that an
  exact data-member-pointer target beats adding member `const` for equal-owner
  targets; the focused conversion-operator gate passes both architectures and
  runtime execution.
- [x] Do not let cv-insensitive type compatibility classify an unsafe nested
  qualification as an identity conversion. Reject `int* C::*` to
  `const int* C::*`, rank valid same-owner member qualification as a conversion,
  and verify a GCC-confirmed overload set chooses the exact target. The focused
  gate passes i686/AMD64 generation and execution.
- [x] Extend member-type ranking to array data members: for a conversion
  function returning `int (C::*)[2]`, select the exact target over
  `const int (C::*)[2]`. GCC C++20 and the focused conversion-operator gate
  confirm the result; the full native-Windows `test-cxx` aggregate passes.
- [x] Preserve owner-path viability after a user-defined conversion:
  conversion-function results cannot convert to a base-member pointer through
  a private or virtual base. GCC C++20 and the focused gate agree; i686/AMD64
  diagnostics and absent-object checks pass for both cases.
- [ ] Extend ranking to other nested member-type shapes, member-function
  parameter/signature variations, and owner-path access-context ranking;
  proper subsequences and full template partial ordering also remain open.

## C++ regression follow-up (2026-10-14, member-pointer owner/cv ordering)

- [x] Compare related destination owners for viable data-member-pointer
  conversions even when member cv qualification differs. The nearer owner
  wins for both `const`/`volatile` tradeoff directions in ordinary and
  function-template overloads; same-owner qualification ordering remains
  distinct. GCC C++20 agrees; the focused target passes i686/AMD64 generation,
  generated AMD64 execution, and GCC host execution.
- [x] Re-run the full native-Windows MinGW GCC `test-cxx` aggregate after the
  member-pointer owner/cv ordering change; it completed successfully,
  including both target architectures and generated AMD64 execution.

## C++ regression follow-up (2026-10-14, member-pointer access context)

- [x] Carry the active class/friend access context into function-template
  conversion ranking for `Base::*` to derived-owner conversions. A private
  base conversion is viable inside its friend function but rejected outside;
  ambiguous and virtual owner paths remain invalid. GCC C++20 agrees; the
  focused overload, data-member-pointer, and member-function-pointer gates
  pass, including both target architectures and generated AMD64 execution.
- [x] Re-run the full native-Windows MinGW GCC `test-cxx` aggregate after the
  member-pointer access-context change; it completed successfully, including
  both target architectures and generated AMD64 execution.

## C++ regression follow-up (2026-10-10, ordinary derived-pointer ranking)

- [x] For equal Conversion-rank ordinary pointer arguments to related class
  targets with matching pointee cv, prefer the more-derived target (`Leaf*` to
  `Middle*` over `Root*`) in a non-template overload set. Keep sibling-base
  targets ambiguous. The focused overload target passes i686/AMD64 generation,
  generated AMD64 execution, expected ambiguity/no-object checks, and GCC C++20
  positive and negative comparisons.
- [x] Apply equal-rank qualification-subset ordering to ordinary conversions
  to `void*` targets: `int*` selects `void*` over `const void*`, while
  `const void*` versus `volatile void*` remains ambiguous. GCC C++20 agrees;
  the same focused gate verifies both target widths and generated AMD64
  execution, plus negative diagnostics without object output.
- [x] Compare qualification subsets through nested ordinary pointer targets:
  `int**` selects `int* const*` over `const int* const*`; incomparable
  `const int* const*` and `volatile int* const*` targets remain ambiguous.
  GCC C++20 agrees; the focused gate passes both architectures, generated
  AMD64 execution, and negative diagnostic/no-object checks.
- [x] Prefer an ordinary pointer-to-base conversion (`Leaf*` to `Root*`) over
  a pointer-to-void conversion at equal Conversion rank. Keep the two unrelated
  base-pointer candidates ambiguous even when `void*` is also viable. GCC C++20
  agrees; the focused target passes both architectures, host execution, and
  expected ambiguity diagnostics with no object output.
- [x] Cover equal-rank derived-to-base pointer conversions where only one
  target adds pointee `const`: `Leaf*` selects `Root*` over `const Root*` for
  both ordinary overloads and function-template candidates. The focused target
  passes i686/AMD64 generation, freestanding i686 and AMD64 host execution, and
  GCC C++20 `-Werror`.
- [ ] Continue proper-subsequence ordering for other standard-conversion
  shapes with different conversion components; user-defined conversion ties
  and complete function-template partial ordering remain open.

## C++ regression follow-up (2026-10-10, fixed-enum promotion ranking)

- [x] Treat conversion from an unscoped fixed-underlying enum to either its
  underlying type or that type's integral promotion as Promotion rank, and
  prefer the underlying-type destination when both overloads are viable. Apply
  the ordering in ordinary overloads and function-template candidates. The
  focused gate passes i686/AMD64 generation, generated AMD64 execution, and
  GCC C++20 `-Werror` execution.
- [ ] Extend conversion-sequence ordering to reference bindings and other
  promotion/conversion pairs; proper subsequences, user-defined conversion
  ties, and complete function-template partial ordering remain open.

## C++ regression follow-up (2026-10-10, pointer-to-bool overload ranking)

- [x] Extend the same-rank pointer-to-bool ordering to ordinary overloads:
  converting a pointer to `void*` is better than converting it to `bool`, as
  required by [over.ics.rank]. The focused gate passes i686/AMD64 generation,
  AMD64 execution, and GCC C++20 `-Werror` execution.
- [x] Keep `nullptr_t` distinct from pointer types in implicit conversions:
  reject `nullptr_t -> bool` for overload arguments while preserving null
  pointer conversion to `void*`. Ordinary and function-template overloads
  select the `void*` candidate, and `static_cast<bool>(nullptr)` remains valid.
  The same focused gate passes for both targets and GCC C++20.
- [x] Apply the same pointer-to-bool tie-break after array-to-pointer decay.
  An array argument competing for `bool` and `void*` previously produced a
  false ambiguity; the `void*` overload now wins, matching GCC C++20. The
  focused native-Windows overload gate passes for i686/AMD64 generation and
  AMD64 execution.
- [ ] Extend equal-rank standard-conversion ordering to other conversions and
  reference bindings; proper subsequences and user-defined conversion ties
  remain open.

## C++ regression follow-up (2026-10-10, noexcept function-pointer conversions)

- [x] Parse `noexcept` on ordinary function-pointer declarators and rank the
  standard conversion from a `noexcept` function pointer to the otherwise-
  identical potentially-throwing function pointer below an identity match.
  Ordinary and function-template overload sets choose the `noexcept` target;
  the focused overload gate passes i686/AMD64 generation, AMD64 execution,
  and GCC C++20 `-Werror` checks.
- [x] Preserve `noexcept` function types in Itanium mangling (`Do`) and
  propagate constant `noexcept(true/false)` specifications onto ordinary
  function declaration types. Regression checks cover direct function-name
  conversion, explicit function-pointer types, ordinary/template overloads,
  distinct overload symbols, both target widths, generated AMD64 execution,
  and GCC C++20 `-Werror`.
- [x] Resolve overloaded free-function names against contextual function-
  pointer parameters during ordinary and function-template overload ranking;
  defer argument analysis until the winning target type is known. The focused
  gate passes with the non-matching overload declared first, on both target
  widths, generated AMD64 execution, and GCC C++20 `-Werror`.
- [x] Preserve explicit global qualification during expression lookup and
  resolve namespace-local overloaded free functions from contextual function-
  pointer types, including ordinary calls, function-template calls, and local
  initializers. Verify namespace and `::global` overloads select distinct
  functions, with both target widths, generated AMD64 execution, and GCC C++20
  `-Werror`.
- [x] Merge namespace-level using-declarations and using-directives into
  contextual function-pointer overload lookup. Verify namespace-imported
  overload sets in local initializers, ordinary calls, and function-template
  calls; both target widths, generated AMD64 execution, and GCC C++20 `-Werror`
  pass.
- [x] Resolve overloaded function-template names as contextual function
  values using target-type deduction, existing constraint/partial-ordering
  logic, and pack expansion. Resolve `noexcept(true)`, `noexcept(false)`, and
  dependent `noexcept(sizeof(T) ...)` on instantiated candidates; combine
  ordinary and template candidates by conversion rank and non-template
  preference. The regressions pass i686/AMD64 generation, AMD64 execution, and
  GCC C++20 `-Werror` execution.
- [x] Include function-template candidates introduced through namespace-level
  using-declarations and using-directives in contextual function-pointer
  lookup. Verify imported templates in pointer initializers through both forms;
  the focused gate passes i686/AMD64 generation, AMD64 execution, and GCC C++20
  `-Werror` execution.
- [x] Allow ordinary functions and function-template specializations with
  matching parameter types but differing exception specifications to coexist.
  In contextual function-pointer lookup, prefer a matching non-template per
  `[over.over]`, but select the `noexcept` template when a potentially-throwing
  ordinary function cannot match the target. The focused gate passes i686/
  AMD64 generation, AMD64 execution, and GCC C++20 `-Werror` execution.
- [ ] Complete remaining C++20 overload-resolution conformance.

## C++ regression follow-up (2026-10-10, reference qualification ranking)

- [x] Compare same-rank reference-binding sequences by their qualification
  subsets when both candidates bind the same reference category, for ordinary
  and function-template overloads. With an `int` lvalue, `const int&` wins
  over `const volatile int&`; the ordinary case reproduced an RCC ambiguity.
  Both cases agree with GCC C++20. Nested pointer-reference targets also
  prefer `int* const&` over `const int* const&` for an `int*` lvalue, for both
  ordinary and function-template overloads. The focused overload gate passes
  i686/AMD64 generation and execution, GCC C++20 `-Werror`, and existing
  ambiguity diagnostics. It also preserves ambiguity for incomparable
  `const int&` versus `volatile int&` candidates in ordinary and function-
  template overloads, with no object output and matching GCC diagnostics. The
  full native-Windows `test-cxx` aggregate passes for the ranking change.
- [x] Correct the positive function-template fixture that passed `nullptr`
  to a `bool` parameter: use explicit `static_cast<bool>(nullptr)`, which GCC
  accepts, while preserving the separate regression that implicit
  `nullptr_t`-to-`bool` overload conversion is rejected.
- [ ] Complete remaining C++20 overload-resolution conformance; proper
  subsequences, user-defined conversion ties, implicit-object refinements, and
  full function-template partial ordering remain open.

## C++ regression follow-up (2026-10-10, cv-qualified derived-reference ranking)

- [x] When equal-rank reference conversions target related classes, compare
  the referred-to class relationship even if one candidate adds cv
  qualification. For a `ConversionBaseLeaf` lvalue, both ordinary and
  function-template overload sets now prefer `ConversionBaseMiddle&` over
  `const ConversionBaseRoot&`. This fixes a reproduced RCC ambiguity and
  agrees with GCC C++20. The reverse cv direction (`ConversionBaseRoot&`
  versus `const ConversionBaseMiddle&`) also selects the nearer class in both
  overload forms. The focused overload target passes i686/AMD64 generation,
  AMD64 execution, GCC C++20 `-Werror`, and its existing ambiguity/diagnostic
  checks.
- [ ] Continue proper-subsequence coverage for other standard conversion
  shapes, user-defined conversion ties, implicit-object refinements, and full
  function-template partial ordering; this derived-reference rule is bounded
  and does not complete overload-resolution conformance.

## C++ regression follow-up (2026-10-10, cv-qualified derived-pointer ranking)

- [x] Compare equal-rank pointer conversions to related class targets even
  when their cv qualifications differ. For `ConversionBaseLeaf*`, ordinary
  and function-template overloads select `ConversionBaseMiddle*` over
  `const ConversionBaseRoot*`; they also select a const-qualified
  `ConversionBaseMiddle*` over a `volatile ConversionBaseRoot*`. GCC C++20
  confirms both results. The focused overload gate passes i686/AMD64
  generation, AMD64 execution, GCC C++20 `-Werror`, and existing ambiguity
  diagnostics.
- [ ] Continue proper-subsequence coverage for other standard conversion
  shapes, user-defined conversion ties, implicit-object refinements, and full
  function-template partial ordering; this pointer-ranking rule is bounded
  and does not complete overload-resolution conformance.

## C++ regression follow-up (2026-10-10, array-reference bound ranking)

- [x] Keep references to arrays of known and unknown bound as distinct
  overload parameter types, and apply the standard equal-rank reference-binding
  tie-break so a reference to the matching known bound wins, even when the
  unknown-bound candidate otherwise has fewer cv qualifiers. Cover ordinary
  and function-template overloads with GCC C++20 oracle checks; the focused
  gate passes i686/AMD64 generation, generated AMD64 execution, and GCC C++20
  `-Werror` execution.
- [ ] Continue proper-subsequence coverage for other standard-conversion
  shapes; user-defined conversion ties and complete function-template partial
  ordering remain open.

## C++ regression follow-up (2026-10-11, user-defined conversion reference binding)

- [x] Include public implicit conversion operators when checking viability for
  reference parameters. For equal-rank sequences that bind the same
  conversion result, prefer an rvalue-reference binding to the conversion
  prvalue over `const T&`; the regression selects `ReferenceConversionResult&&`
  over `const ReferenceConversionResult&`. `test-cxx-conversion-operator`
  passes i686/AMD64 generation and execution, GCC C++20 `-Werror`, and its
  verified-backend object-generation checks.
- [x] Select ref-qualified conversion operators from the source object's
  lvalue/rvalue category and implicit-object cv binding, so `operator T() &`
  and `operator T() &&` produce their respective results for lvalue and
  rvalue sources. The focused conversion-operator target and the full
  function-template-overload target pass for i686/AMD64 with GCC C++20
  comparisons. The complete `mingw32-make SHELL=cmd.exe test-cxx` aggregate
  also passes.
- [x] Retain the selected implicit conversion-function identity through
  ordinary and function-template overload comparisons. Candidates using the
  same conversion function compare their trailing standard conversion
  sequences; candidates using different conversion functions are not ordered
  by those trailing ranks. GCC C++20 confirms both the `Base*` versus `void*`
  selection after one `operator Derived*()` and ambiguity for distinct
  conversion functions, including function-template overloads. The focused
  gate passes i686/AMD64 generation, AMD64 execution, and negative diagnostics
  with no output object; the full `test-cxx` aggregate passes.
- [x] Re-run dependent nested-`typename` regressions after the semantic
  changes. Class-template positives pass i686/AMD64 generation and AMD64
  execution; function-template invalid fixtures report exactly six expected
  diagnostics on both targets, and fallback positives compile for both.
  The native-Windows `test-cxx` aggregate also passes. No nested-type parser
  change was needed.
- [x] Parse and lower conversion-function templates in non-template classes.
  Deduce template arguments from the required conversion result, evaluate
  supported `requires` constraints, instantiate a real member specialization
  with the implicit-object ABI, and carry its identity into existing user-
  defined conversion ranking. The focused test checks direct and argument
  conversions, `sizeof(T)` constraint filtering against `int`/`double`
  overloads, and GCC-confirmed ambiguity for an unconstrained `operator T()`.
  `test-cxx-conversion-operator` passes GCC C++20 `-Werror`, i686/AMD64
  assembly/object generation, generated AMD64 execution, verified-backend
  object generation, and ambiguous-diagnostic/no-output checks for both
  targets.
- [x] Re-run dependent nested-`typename` regressions after the parser changes.
  `test-cxx-class-template-dependent-nested-type` passes on native Windows,
  including exact class/function-template diagnostics, i686/AMD64 generation,
  and generated-code execution. There was no uncommitted parser/fixture diff in
  the child checkout to carry forward.
- [x] Implement ordinary named member-function templates in non-template
  classes. Deduce from call arguments; materialize real static and
  non-static specializations; rank them with non-template overloads, defaulted
  integral template arguments, supported parenthesized and unparenthesized
  `requires` clauses, and member-template partial ordering. Match GCC's
  Itanium ABI spellings for dependent parameter and return types, pointer
  parameters, static methods, and defaulted non-type arguments. The focused
  target passes GCC C++20 `-Wall -Wextra -Werror`, i686/AMD64 assembly and
  object generation, generated AMD64 execution, exact symbol checks, and
  verified-backend output on both targets. The full native-Windows
  `test-cxx` aggregate also passes after this implementation.
- [x] Instantiate named member-function templates within concrete
  class-template specializations. Substitute enclosing class type parameters
  through method declarations and bodies; preserve method cv/ref qualifiers,
  staticness, default non-type arguments, and class-dependent `requires`
  clauses. The focused Windows gate executes Box specializations for two class
  argument types, checks fourteen GCC-Itanium symbols (including static,
  deduced/explicit type arguments, and defaulted/explicit non-type arguments),
  emits/assembles i686 and AMD64 objects, executes generated AMD64 code, and
  passes verified-backend output for both targets. A first full `test-cxx`
  rerun stopped when E: ran out of space during `objcopy`; the subsequent full
  rerun after the explicit-argument work passed.
- [x] Parse explicit type and integral non-type template arguments on concrete
  class-template member calls, materialize the matching specialization, and
  allow standard argument conversions after the explicit type is fixed. The
  regression includes `add<short>(2)`, `add_constant<3>()`, and a comparison
  expression before template calls to guard the `<` lookahead. Also parse and
  substitute a dependent `this->template echo<T>(...)` call in a class-template
  method. The focused gate passes GCC C++20 `-Wall -Wextra -Werror`, fourteen
  exact Itanium symbol checks against GCC, i686/AMD64 assembly and object
  generation, generated AMD64 execution, and verified-backend output for both
  targets. The nested dependent-`typename` gates retain their exact diagnostic
  counts on both architectures. The complete native-Windows `test-cxx`
  aggregate passes.
- [x] Resolve explicit and deduced member-template calls through a dependent
  `.` object (`other.template echo<T>(value)` and
  `other.template echo(value)`) after class substitution. Reuse a materialized
  specialization without duplicating its ABI `this` parameter, substitute the
  injected-class-name parameter to the active class specialization, and
  preserve GCC-compatible Itanium mangling for `const Box&`. Both exact symbols
  are checked against GCC; the gate passes GCC C++20 execution, i686/AMD64
  assembly and object generation, and verified-backend output on both targets.
  A negative regression confirms an incompatible return remains diagnosed
  after dependent call substitution on both architectures. Expanded name
  buffers also keep deeply nested template friend-function mangling working.
  Native-Windows `test-cxx` and `test-c17` aggregates pass.
- [x] Support function-parameter packs and non-type template packs in named
  member-function templates of class-template specializations. Preserve the
  nested member-template pack during enclosing-class substitution, defer its
  fold expression until member-template specialization, and emit GCC-matching
  Itanium pack encodings for template arguments and the `Dp` function-parameter
  pattern, including explicit empty member-template argument lists. Regressions
  cover empty, homogeneous, and heterogeneous deduced `Rest...` packs, plus
  empty and non-empty `add_constants` folds and `sizeof...(Args)` for empty
  and heterogeneous argument sequences. Deduce a pack through the nested
  `PackPair<Args, Args>...` pattern, enforce the repeated type, and preserve
  the nested template-id in GCC-matching Itanium mangling. Deduce scalar `Tag`
  from `PackTagged<Args, Tag>...` across each pack element, require one
  consistent value, and encode the dependent argument as GCC's `XT_E`; exact
  symbols match GCC. Mismatched `PackPair<int, long>` and mixed-tag
  `PackTagged<int, 7>, PackTagged<long, 8>` arguments are rejected on both
  targets without producing an object (matching GCC). Evaluate explicitly
  supplied `Tag` in a nested `Tag + 1` NTTP pattern; its `XplT_Li1E` expression
  encoding matches GCC exactly, and tags that do not equal `Tag + 1` are
  rejected on both targets. The focused gate passes
  i686/AMD64 assembly and object generation, generated AMD64 execution, GCC
  C++20 `-Wall -Wextra -Werror`, and verified-backend output for both targets.
  The complete native-Windows `mingw32-make SHELL=cmd.exe test-cxx` and
  `test-c17` aggregates exit 0 after these additions.
- [x] Encode dependent integer division and remainder in nested member-template
  function-parameter pack patterns (`Tag / 2` and `Tag % 4`). Emit the Itanium
  `dv` and `rm` expressions matching GCC exactly (`XdvT_Li2E` and
  `XrmT_Li4E`). Positive calls execute; mismatched quotient and remainder
  arguments are rejected on both targets without object output. The focused
  gate and complete native-Windows `mingw32-make SHELL=cmd.exe test-cxx`
  aggregate pass.
- [x] Encode dependent bitwise AND/OR/XOR and left/right shifts in nested
  member-template function-parameter pack patterns. Emit the Itanium `an`,
  `or`, `eo`, `ls`, and `rs` expressions matching GCC exactly. Positive calls
  execute; mismatched results are rejected on both targets without object
  output. The focused gate and complete native-Windows
  `mingw32-make SHELL=cmd.exe test-cxx` aggregate pass.
- [x] Encode dependent unary `!`/`~`, comparison and logical expressions, and
  conditional expressions in nested member-template pack patterns. Emit
  GCC-matching Itanium `nt`, `co`, comparison/logical, and `qu` encodings.
  Track parenthesis depth so `>` inside a non-type template expression remains
  a relational operator while an unparenthesized `>` closes the template-id.
  Positive and mismatched-value tests cover both targets; exact symbols match
  GCC, with no object output for mismatches. Native-Windows `test-cxx` and
  `test-c17` aggregates both exit 0.
- [x] Encode dependent integral `static_cast` expressions in nested
  member-template pack patterns, using the Itanium `sc` operator and the
  destination type. The full symbol matches GCC; valid calls execute and an
  incorrect cast result is rejected on both targets without object output.
  The focused gate and complete native-Windows `test-cxx` aggregate pass.
- [x] Encode dependent integral C-style casts in nested member-template pack
  patterns with the Itanium `cv` operator. GCC symbol matching, positive
  execution, and mismatched-value rejection without object output pass on the
  focused i686/AMD64 gate.
- [x] Deduce nested member-template parameter packs whose element pattern uses
  `sizeof...(Args)` as a dependent non-type template argument. Emit the Itanium
  `sZ` pack-size expression with GCC-identical symbols, handle both empty and
  non-empty packs, and reject an element whose encoded size does not match the
  deduced pack. The focused i686/AMD64 gate, complete native-Windows `test-cxx`
  aggregate, and complete `test-c17` aggregate pass.
- [x] Deduce nested member-template type packs whose non-type argument uses
  `sizeof(Args)`. Substitute each deduced element type before constant
  evaluation and emit GCC-identical Itanium `st`/type-substitution encodings;
  verify empty and heterogeneous packs, and reject `PackTagged<int, 8>` when
  the pattern requires `sizeof(int) == 4` on i686/AMD64 and GCC. The focused
  target and full native-Windows `test-cxx` aggregate pass. RCC source commit
  `74da05a22516e00cacb0c36e62cc44081686c03d` is on local `main`, not pushed.
- [x] Parse explicit specializations of named member-function templates owned
      by non-template classes. Match explicit type and integral template
      arguments (including deduction when the template-id is omitted) plus
      return/parameter types, variadicness, cv/ref qualifiers, and `noexcept`
      against the declared member template; preserve the specialization
      instance when its primary's out-of-class definition is parsed later;
      and support a specialization declaration followed by its definition.
      A specialization does not inherit the primary's `constexpr`/`consteval`
      specifier. The focused regression covers an overload set and matches
      GCC's Itanium symbols, assembles i686 and AMD64 outputs, and executes the
      generated AMD64 result; existing
      `test-cxx-function-templates`, `test-cxx-class-template-methods`, and
      `test-cxx-class-template-specialization` gates pass.
- [x] Parse `inline`, `constexpr`, and `consteval` on out-of-class
      member-template definitions; require exact `constexpr`/`consteval`
      agreement on qualified method, constructor, destructor, and conversion
      definitions. Explicit specializations remain independent and do not
      inherit the primary's specifiers. Positive i686/AMD64 generation, GCC
      symbol comparison, and AMD64 execution pass in
      `test-cxx-member-function-template-explicit-specialization`; mismatch
      diagnostics for each qualified-definition category pass on both widths
      in `test-cxx-member-methods-invalid`, along with the existing
      class/function-template regression gates.
- [ ] Complete member-template deduction and overload selection for remaining
      general dependent non-type expressions and template-template forms,
      expansions involving multiple distinct packs and other nested patterns,
      and remaining overload edge cases; support constructor templates,
      explicit specializations on class-template owners and for overloaded
      operators, inherited conversion templates, full C++20 constraint
      subsumption, and complex dependent Itanium ABI substitutions.
      Conversion target deduction and user-defined sequence ranking are
      specified in
      [`temp.deduct.conv`](https://timsong-cpp.github.io/cppwp/std20/temp.deduct.conv)
      and [`over.ics.rank`](https://timsong-cpp.github.io/cppwp/std20/over.ics.rank).

## RCC continuation (2026-10-11, SysV flexible-array ABI and native Clang tests)

- [x] Classify structs with flexible array members as memory-passed SysV AMD64
  aggregates for both arguments and returns. Add a round-trip ABI regression;
  the focused gate passes with both MSYS2 Clang and MinGW GCC. The Windows GCC
  host uses an explicit ABI harness because its `sysv_abi` C caller does not
  agree with the SysV flexible-array convention; the test still exercises the
  generated RCC code and checks both argument and hidden-result paths.
- [x] Make the native-Windows Clang i686 freestanding test runner rename the
  input COFF `main` symbol to `_main`, including when the entry harness is the
  second input. This fixes Clang/LLD execution without changing RCC artifacts
  or the GCC/MinGW path. The Clang test-ci targets through direct relocation
  and the focused flexible-array ABI target pass; the dependent nested-type
  C++20 target also passes with GCC. No parser/fixture diff remains.
- [x] Allow `scripts/bootstrap_gate.sh` to select the built RCC/RLD with `RCC`
  and `RLD` environment variables. With the current native Windows build,
  bootstrap `core` (38 sources, i686 and x86_64, duplicate object comparison)
  and `link` (stage1-a/b images, 33 objects per target) both pass.
- [x] Run the bootstrap stage1 execution and stage2 reproduction gates on the
  Ubuntu-24.04 WSL host. The full `make SHELL=cmd.exe test-ci` run on
  2026-10-12 passed stage1 execution and byte-identical stage2 object/image
  reproduction for i686 and x86_64; the earlier WSL/MinGW limitations are
  superseded.

## RLD scalability follow-up (2026-10-11, indexed linker state)

- [x] Replace per-insertion symbol/relocation tail scans with maintained tail
  pointers, and add a dynamically resized hash index for global symbol lookup
  while preserving the ordered symbol list used for deterministic output.
  GCC and Clang both pass link, archive-selection, weak replacement, COMDAT,
  and format-validation tests. Re-link all four 33-object stage1 images with
  each RLD: total link time falls from about 4–5 minutes per image to about
  13 seconds for all four, with byte-identical SHA-256 outputs across the
  before/after versions and between stage1-a/stage1-b.

## RCC continuation (2026-10-11, pointer-to-array extent partial ordering)

- [x] Preserve fixed array-bound specialization through pointer and nested
  array layers when comparing the original function-template parameter
  patterns. `T (*)[1]` now outranks `T (*)[N]` for a one-element-array pointer;
  nested multidimensional array pointers are covered as well. Crossed patterns
  specializing opposite parameters remain ambiguous and emit no object.
- [x] Verify GCC C++20 oracle behavior, i686/AMD64 code generation and AMD64
  execution, crossed-ambiguity diagnostics with no output object, the focused
  `test-cxx-function-template-overloads` gate, and the complete native-Windows
  `mingw32-make SHELL=cmd.exe test-cxx` aggregate.

## RCC continuation (2026-10-11, fixed non-type function pattern ordering)

- [x] Prefer a dependent class-template-id pattern with a fixed integral
  argument (`Tag<First, 7>`) over the otherwise matching independent-parameter
  pattern (`Tag<First, Second>`). GCC C++20 confirms selection; RCC i686/AMD64
  generated execution and the focused overload gate pass. Crossed fixed-value
  patterns remain ambiguous under GCC and RCC and produce no RCC object.
- [x] Re-run the complete native-Windows `mingw32-make SHELL=cmd.exe test-cxx`
  aggregate after this ordering change; all C++ regressions pass (exit 0).

## RCC continuation (2026-10-11, pointer-pack partial-ordering regressions)

- [x] Gate the C++20 comparison of `Types*...` with `First*, Rest*...`:
  the all-pack overload wins for an empty call, while the fixed-first pattern
  wins for one through three pointer arguments with heterogeneous pointee
  types. Also verify crossed `First*, Rest...` / `First, Rest*...` patterns
  remain ambiguous and emit no object. GCC C++20 and the RCC i686/AMD64
  focused overload target pass; this target is part of `test-cxx` CI coverage.
- [ ] Continue general C++20 function-template partial ordering across
  transformed-template deduction, remaining pack shapes, dependent
  non-type/template-template parameters, constraints, and conversion ties.

## RCC continuation (2026-10-11, non-trailing function parameter packs)

- [x] Treat a function parameter pack before a fixed suffix as non-deduced
  during call deduction; with no explicit pack arguments it remains empty and
  the suffix consumes the next call argument. Explicit one- and two-element
  packs shift suffix argument mapping correctly, while an extra call argument
  with an undeduced empty pack is rejected. GCC C++20 and the focused RCC
  overload gate pass for i686/AMD64 generation and AMD64 execution; invalid
  compilation emits no object. The full native-Windows
  `mingw32-make SHELL=cmd.exe test-cxx` suite also passes (exit 0).
- [x] Parse dependent `typename Owner::value_type` in nested function-pointer
  parameter declarators and expand that declaration as a function parameter
  pack. Verify exact GCC/RCC Itanium symbols on i686/AMD64 and execute the
  generated x86_64 fixture; the focused native-Windows regression passes.
- [x] Re-run the full native-Windows `test-cxx` aggregate after this parser fix;
  the complete suite passed (exit 0).
- [x] When explicit template arguments fill a non-final function parameter
  pack, leave following non-pack template parameters available for deduction
  from their corresponding call arguments. Verify `Rest*...` with two explicit
  pack types and a separately deduced suffix type against GCC C++20; the
  focused overload gate passes i686/AMD64 generation and execution, and an
  ill-matched pack element is rejected without producing an object.
- [ ] Extend function-pack deduction to remaining pack-expansion shapes and
  interactions with other deduction sources; full C++20 partial ordering and
  deduction remain incomplete.

## RCC continuation (2026-10-12, class-template-id type-pattern ordering)

- [x] Substitute dependent type arguments stored on instantiated class-template
  identities when building function-template specializations, then compare
  matching `Box<T*>` and `Box<T>` patterns by transformed-template deduction.
  `Box<int*>` selects `Box<T*>`, while `Box<int>` selects `Box<T>` over a
  template-template fallback. Crossed `First*, Second` / `First, Second*`
  patterns remain ambiguous.
- [x] Verify GCC C++20 behavior and the focused
  `test-cxx-function-template-overloads` gate, including RCC i686/AMD64
  generation and AMD64 execution plus ambiguous-diagnostic/no-object checks.
- [x] Rerun the complete native-Windows
  `mingw32-make SHELL=cmd.exe test-cxx` suite on the final source; all C++
  regression tests pass (exit 0).
- [ ] Continue general C++20 function-template partial ordering across
  remaining transformed-template shapes, packs, dependent non-type and
  template-template parameters, constraints, and conversion ties.

## RCC continuation (2026-10-12, class-template type packs in function ordering)

- [x] Preserve structural matching for class templates with a trailing type
  parameter pack when deducing and ordering function-template patterns. A
  `PackBox<T*>` overload now outranks `PackBox<T>` when instantiated from a
  class declared as `template<typename... Types>`, while the general pattern
  remains selected for non-pointer types.
- [x] Verify the GCC C++20 oracle and focused overload gate, including i686
  and AMD64 code generation, AMD64 execution, and existing invalid-overload
  diagnostics.
- [x] Complete the full native-Windows
  `mingw32-make SHELL=cmd.exe test-cxx` regression suite after this change.
- [ ] Continue general C++20 function-template partial ordering across
  remaining transformed-template shapes, pack arities and placements,
  dependent non-type/template-template parameters, constraints, and ties.

## RCC continuation (2026-10-12, fixed non-type prefix with class type packs)

- [x] Preserve class-template identity arguments when a trailing type pack
  follows fixed class-template parameters, including zero and multiple pack
  elements.
- [x] Deduce and order overloads using a fixed integral class-template
  argument followed by a type pack. GCC C++20 and RCC agree that
  `PackBox<7, Types*...>` beats `PackBox<7, Types...>` for empty, one-element,
  and multi-element pointer packs, while the plain pack handles non-pointer
  types.
- [x] Verify the focused overload gate and full native-Windows
  `mingw32-make SHELL=cmd.exe test-cxx` regression suite; both pass (exit 0),
  including GCC oracle checks, i686/AMD64 generation, AMD64 host execution, and
  existing ambiguity/no-output diagnostics.
- [x] Extend class-pack ordering to direct trailing integral non-type and
  template-template pack expansions after a fixed leading argument. Verify
  empty, one-element, and multiple-element cases against GCC C++20 and execute
  both RCC target programs on native Windows.
- [ ] Extend class-pack ordering to other dependent non-type expression forms,
  compound/repeated/mixed pack sources, and broader template-template pack
  signatures; full C++20 partial ordering, constraints, and conversion ties
  remain open.

## RCC continuation (2026-10-12, cv-sensitive template argument identity)

- [x] Keep class-template specializations with nested cv-qualified type
  arguments distinct in the instantiation cache and type identity checks;
  order `Box<T*>` against `Box<const T*>` to match GCC C++20 for both
  `Box<int*>` and `Box<const int*>`.
- [x] Verify the focused overload gate, including GCC oracle, i686/AMD64 code
  generation, AMD64 execution, and ambiguity/no-object diagnostics.
- [x] Run the complete native-Windows
  `mingw32-make SHELL=cmd.exe test-cxx` suite after the identity change.
- [ ] Continue general C++20 function-template partial ordering across
  remaining transformed-template shapes, packs, dependent non-type and
  template-template parameters, constraints, and conversion ties.

## RCC continuation (2026-10-12, multi-argument class-pack ordering)

- [x] Extend function-template ordering coverage to a trailing class type pack
  instantiated with multiple actual arguments. `PackBox<T*, int>` is preferred
  over `PackBox<T, int>` for `PackBox<int*, int>`, while the general pattern
  remains selected for `PackBox<int, int>`.
- [x] Verify the GCC C++20 oracle and focused overload gate, including i686 and
  AMD64 generation, AMD64 execution, and the existing ambiguity/no-output
  diagnostics.
- [x] Run the complete native-Windows
  `mingw32-make SHELL=cmd.exe test-cxx` regression suite after this change; it
  completed successfully (exit 0).
- [x] Parse and implement the bounded trailing type-pack form nested in a
  class-template-id, such as `PackBox<First, Rest...>`. Deduction now captures
  zero, one, or multiple `Rest` types and function-template substitution
  rebuilds the class-template argument list before specialization.
- [x] Verify empty, single-element, and multi-element expansions against GCC
  C++20; the focused overload gate passes for i686 and AMD64 generation and
  AMD64 execution, including existing ambiguity/no-object diagnostics.
- [x] Implement bounded transformed-template ordering between
  `Box<Types...>` and `Box<First, Rest...>` when both function parameters use
  the same class-template-id and directly expand one trailing type pack. The
  nonempty case prefers the fixed-first pattern; `Box<>` remains viable only
  for the all-pack pattern.
- [x] Permit zero actual class-template arguments when deducing a direct
  trailing type-pack expansion, and verify empty, single-element, and
  multi-element calls against GCC C++20.
- [x] Run the focused
  `mingw32-make SHELL=cmd.exe test-cxx-function-template-overloads` gate and
  the complete native-Windows `mingw32-make SHELL=cmd.exe test-cxx` suite; both
  pass (exit 0), including i686/AMD64 generation and AMD64 execution.
- [ ] Extend ordering beyond direct trailing type packs to compound/repeated or
  mixed pack sources, dependent non-type expressions beyond the fixed
  `sizeof(Type)` prefix implemented below, non-type/template-template packs,
  and interactions with constraints and conversion-sequence ties.

## RCC continuation (2026-10-12, dependent sizeof prefix with class type packs)

- [x] Deduce and order a fixed dependent non-type class-template argument
  `sizeof(Head)` before the fixed class type `Head` and trailing type pack;
  preserve the function-template parameter identity when class-template
  parameter indices overlap.
- [x] Compare empty, pointer-tail, and mixed-tail overload selection against
  GCC C++20; verify i686 and AMD64 generation, AMD64 host execution, and
  existing invalid/ambiguous overload diagnostics.
- [x] Run the focused
  `mingw32-make SHELL=cmd.exe test-cxx-function-template-overloads` gate and
  the complete native-Windows `mingw32-make SHELL=cmd.exe test-cxx` suite;
  both completed successfully (exit 0).
- [ ] Extend fixed dependent non-type ordering beyond `sizeof(Type)` and
  structurally matched arithmetic expressions to other expression shapes,
  non-type packs, and template-template packs; full C++20 partial ordering,
  constraints, and conversion ties remain open.

## RCC continuation (2026-10-12, arithmetic dependent-value class-pack ordering)

- [x] Structurally compare arithmetic dependent non-type expression trees
  during transformed function-template ordering, recursively deducing
  `sizeof(Head)` operands and matching constant leaves. This fixes GCC-confirmed
  pointer-pack preference for a fixed `sizeof(Head) + 1` class-template prefix.
- [x] Verify empty, pointer-tail, and mixed-tail calls against GCC C++20;
  focused and full `test-cxx` pass with i686/AMD64 generation, AMD64 execution,
  and existing invalid/ambiguous overload diagnostics.
- [ ] Extend expression-tree ordering beyond the exercised arithmetic form to
  dependent calls/member expressions and other NTTP structural equivalence
  cases; non-type/template-template packs and full ordering remain open.

## RCC continuation (2026-10-12, dependent static-member value ordering)

- [x] Keep a dependent `Head::value` non-type argument dependent until `Head`
  is deduced, then resolve and fold an actual initialized `static constexpr`
  member; compare its overload ordering with a fixed literal pattern. GCC
  C++20 confirms the fixed `1` overload wins for value `1`, while the dependent
  member pattern is selected for value `2`.
- [x] Verify focused overload diagnostics and execution against GCC, with i686
  and AMD64 output generation.
- [x] Run the complete native-Windows
  `mingw32-make SHELL=cmd.exe test-cxx` suite; it completed successfully
  (exit 0).
- [ ] Continue dependent call/member expression forms beyond static constexpr
  values and the tested arithmetic compositions, plus NTTP/template-template
  packs and full partial ordering and constraints.

## RCC continuation (2026-10-12, arithmetic static-member value ordering)

- [x] Recognize supported arithmetic/conditional expression trees containing
  a type-dependent static-member value when ordering against a fixed integral
  non-type argument. Require every expression-tree leaf to be supported before
  treating the dependent expression as deducible.
- [x] Compare `Head::value + 1` against fixed value `2` with GCC C++20: the
  fixed overload wins when `Head::value` is `1`, and the dependent overload
  wins when `Head::value` is `3`.
- [x] Compare the conditional expression
  `Head::value == 3 ? 5 : Head::value + 1` against fixed value `2`; GCC and RCC
  choose the fixed overload for value `1` and the dependent expression for
  value `3`.
- [x] Run
  `mingw32-make SHELL=cmd.exe test-cxx-function-template-overloads`; i686 and
  AMD64 generation, AMD64 host execution, GCC execution, and existing
  ambiguity/no-output diagnostics all pass (exit 0).
- [ ] Extend beyond these static-member expression compositions to dependent
  calls and other member forms, non-type/template-template packs, constraints,
  and full C++20 partial ordering.

## RCC continuation (2026-10-12, C/C++ UTF-8 literal character types)

- [x] Recognize C17 `u8"..."` as a byte string literal and preserve ordinary
  `char` array initialization semantics.
- [x] In C++20, give UTF-8 string and character literals their `char8_t` type;
  accept matching `char8_t` arrays and reject mismatched `char` and
  `unsigned char` arrays. Keep `char8_t` an ordinary identifier in C and
  before C++20, including user typedefs with that name.
- [x] Add GCC C17/C++17/C++20 compatibility checks, i686 and AMD64 object
  generation, AMD64 runtime checks, and negative diagnostics; update the
  user-defined UTF-8 literal test to use `char8_t`.
- [x] Run the focused C17 and C++ UTF-8 literal gates, the C17 regression
  suite, the C++ user-defined literal gate, and the complete native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1 test-cxx`
  suite successfully.
- [x] Add support for wide, UTF-16, and UTF-32 literal encodings; the
  implementation and regression coverage are reconciled in the 2026-10-14
  follow-up below.
- [ ] Continue auditing the remaining C++ character-literal and
  execution-character-set rules against GCC and the standard.

## RCC continuation (2026-10-12, UTF-8 character-literal escapes)

- [x] Decode universal-character-name escapes in UTF-8 character literals via
  the UTF-8 encoding path; accept a single-code-unit value such as
  `u8'\u0041'` and diagnose a value that needs multiple UTF-8 code units.
- [x] Verify four-byte supplementary-plane UTF-8 encoding for
  `u8"\U0001F600"`, including all four emitted code units and the terminator
  in the generated AMD64 runtime probe; compile both target architectures.
- [x] Match GCC C++20 syntax acceptance/rejection, assemble i686 and AMD64
  output, and execute the generated AMD64 probe through
  `test-cxx-utf8-literals` (exit 0).
- [x] Extend character literal and escape coverage beyond the bounded UTF-8
  case with wide, UTF-16, and UTF-32 forms (see the 2026-10-14 reconciliation
  below).
- [ ] Complete the remaining execution-character-set rules.

## RCC continuation (2026-10-13, UTF-8 UCN embedded NUL)

- [x] Preserve `u8"\u0000"` as one zero-valued UTF-8 code unit before the
  distinct string terminator, and accept `u8'\u0000'` as a one-code-unit
  character literal. The string token's explicit byte length keeps the
  embedded zero from truncating initialization.
- [x] Compare C++20 syntax with GCC; compile i686/AMD64 and execute the
  generated AMD64 byte, array-size, and character-value checks through
  `test-cxx-utf8-literals` (exit 0).
- [x] Implement the wide, UTF-16, and UTF-32 literal forms; their dual-target
  runtime and format gates are recorded in the 2026-10-14 reconciliation
  below.
- [ ] Continue the remaining execution-character-set conformance.

## RCC continuation (2026-10-13, narrow-string UCN execution encoding)

- [x] Encode universal character names in ordinary narrow strings with the
  RinOS UTF-8 execution character set, sharing the length-aware encoder used
  by `u8` literals. Preserve embedded NUL bytes and distinguish them from the
  final string terminator.
- [x] Compare C17/C++20 bytes and array bounds against GCC with an explicit
  UTF-8 execution charset; compile i686/AMD64 and execute both C17 and C++20
  AMD64 regressions (`test-language-boundaries` and
  `test-cxx-utf8-literals`, exit 0).
- [x] Complete bounded ordinary narrow-character UCN handling for C17/C++20:
  C17 rejects UCNs for prohibited basic/control code points, ordinary narrow
  literals encode UCNs with RinOS's UTF-8 execution encoding, and a value that
  encodes to multiple bytes uses the implementation-defined `int` character
  constant representation. The C++20 `u8` one-code-unit constraint remains
  enforced. Source status: `docs/implementation-status-rcc-narrow-character-literals-v1.md`.
- [x] Add wide, UTF-16, and UTF-32 literal encodings; see the 2026-10-14
  reconciliation below for the implementation and gate evidence.
- [ ] Continue auditing the remaining C++ character-literal and
  execution-character-set rules against GCC and the standard.

## RCC continuation (2026-10-13, ordinary multibyte character constants)

- [x] Encode ordinary multibyte character constants using the RinOS UTF-8
  execution encoding and expose their implementation-defined packed value as
  `int` in both C17 and C++20; keep one-byte and `u8` character literals on
  their existing paths.
- [x] Enforce C17 basic-source-character UCN restrictions while preserving
  the `$`, `@`, and grave-accent exceptions; compare valid values and invalid
  diagnostics with GCC.
- [x] Compile i686/AMD64, execute generated C17 and C++20 AMD64 probes, and
  execute GCC UTF-8 runtime oracles in `test-language-boundaries` and
  `test-cxx-utf8-literals` (both exit 0).
- [x] Implement wide, UTF-16, and UTF-32 literal forms; see the 2026-10-14
  reconciliation below for verification.
- [ ] Continue auditing other execution character sets and their remaining
  literal-conversion rules.

## RCC continuation (2026-10-14, wide/UTF-16/UTF-32 literal audit)

- [x] Reconcile the existing literal implementation with the older unchecked
  items: C17 wide (`L`) and UTF-16/UTF-32 (`u`/`U`) strings; C++11/C++20 wide,
  UTF-16, and UTF-32 strings and character literals; UTF-8/UCN and supplementary
  code points; raw/adjacent strings; mixed-prefix concatenation; embedded NUL;
  and the RinOS 32-bit `wchar_t` representation all have source fixtures.
- [x] Run the native-Windows MinGW GCC gates
  `test-c-wide-literals`, `test-c-utf16-utf32-string-literals`,
  `test-cxx-wide-literals`, and `test-cxx-utf8-literals` (exit 0). These cover
  i686/AMD64 generation, C and C++ runtime probes, verified-backend UTF-16/32
  object generation, GCC compatibility checks, and `.rin` linking/validation.
- [x] Confirm the C tests are in `C17_REGRESSION_TARGETS`, the C++ tests are in
  `CXX_REGRESSION_TARGETS`, and both aggregate gates are dependencies of
  production `test-ci`.
- [ ] Continue the broader execution-character-set and literal conformance
  audit; these bounded encoding tests do not establish full standard
  conformance.

## RCC continuation (2026-10-12, multiline macros and full production gate)

- [x] Expand function-like macro invocations spanning physical source lines,
  treating embedded newlines as preprocessing whitespace while preserving
  diagnostic line numbers; add nested C17/C++20 tests for i686 and x86_64.
- [x] Define UINT64_C by token-pasting the ULL suffix before integer-literal
  parsing, and verify both its unsigned type and full-width decimal value.
- [x] Make Windows CI select Python and invoke WSL through Sysnative when
  running under 32-bit make/WOW64; retain normal System32 and POSIX paths.
- [x] Refresh the format goldens for the current ABI/optimizer output and pass
  make SHELL=cmd.exe test-ci (exit 0), including C17/C++20, IR/MIR, verified
  backend, optimizer, RIN/RLL/DRV validation, deterministic checks, stage1
  execution, and i686/x86_64 stage2 reproduction.
- [ ] Continue the remaining C++20 conformance, typed-SSA/backend coverage,
  optimizer, debug-info, and RinOS-native self-host items tracked above; this
  production-gate pass does not imply general C++20 completeness.

## RCC continuation (2026-10-12, sizeof-pack class-template ordering)

- [x] Recognize `sizeof...(Types)` as a dependent value in a class-template
  argument pattern with a trailing function-template type pack, and substitute
  the deduced pack arity when checking the pattern's fixed non-type arguments.
- [x] Verify GCC C++20-compatible overload selection for zero-, one-, and
  two-element packs; the focused overload gate passes for i686/AMD64 code
  generation and AMD64 execution.
- [x] Run the complete native-Windows `mingw32-make SHELL=cmd.exe test-cxx`
  suite successfully (exit 0).
- [ ] Extend dependent non-type pack ordering beyond `sizeof...(Types)` and
  continue full C++20 function-template partial ordering, constraints, and
  conversion-sequence tie coverage.

## RCC continuation (2026-10-12, dependent constexpr member-call ordering)

- [x] Preserve a dependent `Head::value()` class-template non-type argument
  until `Head` is substituted, then evaluate an unambiguous public static
  zero-argument constexpr integer member through the existing scalar constexpr
  evaluator during class-template argument substitution.
- [x] Compare the dependent call pattern with a fixed integral pattern against
  GCC C++20: the fixed pattern wins for value `1`; the dependent call wins for
  value `2`. Cover empty and multi-type trailing packs, and verify that
  non-constexpr and private members do not hijack the fixed-pattern fallback.
- [x] Pass `test-cxx-function-template-overloads` and the complete `test-cxx`
  prerequisite of `test-ci`, including GCC execution, i686/AMD64 generation,
  AMD64 execution, and existing ambiguity/no-output diagnostics.
- [x] Run all pre-bootstrap `test-ci` targets successfully on this revision,
  including C17/C++20, IR/MIR, verified backend, optimizer, DWARF, RIN/RLL/DRV
  validation, deterministic/property/fuzz gates. The full gate then stopped at
  `test-bootstrap-core` because WSL returned `Wsl/Service/E_ACCESSDENIED`; no
  successful full `test-ci` is claimed for this revision.
- [x] Extend dependent constexpr member-call ordering to unique public static
  constexpr calls with fixed integer-constant arguments and exact arity.
- [ ] Extend beyond fixed integer arguments and the tested single remappable
  scalar function-template NTTP argument; variadic member sets, other
  receiver/member forms and return types, and general C++20
  partial ordering, constraints, and conversion-sequence ties remain open.

## RCC continuation (2026-10-12, constexpr member-call arguments)

- [x] Evaluate a dependent class-template NTTP call such as `Head::value(2)`
  after substitution when it resolves to one public static, non-variadic,
  constexpr integer-returning member with exact arity and integer constant
  arguments; reuse the scalar constexpr function evaluator. Ambiguous overload
  sets and unsupported arguments remain unevaluated rather than guessed.
- [x] Verify GCC C++20 ordering against a fixed integral pattern: the fixed
  pattern wins when the call evaluates to `1`, while the dependent pattern is
  selected for value `3`, with one- and two-type trailing packs. The preceding
  zero-argument regression retains empty-pack coverage.
- [x] Pass `test-cxx-function-template-overloads` and the complete native
  Windows `mingw32-make SHELL=cmd.exe test-cxx` suite (exit 0).
- [x] Order a dependent call pattern with one dependent type-member constant
  argument (`Head::value(ArgumentTag::value)`) against a fixed integral pattern
  after substitution. The regression verifies that the fixed pattern wins for
  value `1` and the dependent pattern wins for value `4`, with one- and two-type
  trailing packs; GCC C++20 accepts the same cases.
- [x] Re-run `test-cxx-function-template-overloads` and the complete native
  Windows `mingw32-make SHELL=cmd.exe test-cxx` suite after this extension
  (exit 0).
- [x] Order a dependent call pattern using one scalar function-template NTTP
  (`Head::value(Argument)`) when that same parameter is deduced in a nested
  class-template argument (`FunctionTemplateValuePackTag<Argument>`). The
  fixed integral pattern wins at value `1`, while the dependent-call pattern
  wins at value `4`; the corresponding fixed-pattern template uses different
  parameter names and parameter positions to verify mapping rather than
  spelling- or same-position-only matching.
- [x] Pass the GCC C++20 oracle, focused overload gate (i686/AMD64 plus host
  execution and diagnostic regressions), and complete native-Windows
  `mingw32-make SHELL=cmd.exe test-cxx` suite (exit 0).
- [x] Verify two independent scalar function-template NTTP mappings in one
  dependent constexpr member-call ordering case. The mappings use different
  parameter names and positions; a two-argument `Head::value(First, Second)`
  distinguishes the fixed-value winner from the dependent-call winner.
- [x] Pass the GCC C++20 oracle, focused overload gate, and complete native
  Windows `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1 test-cxx`
  suite (exit 0), covering i686/AMD64 generation and AMD64 execution.
- [x] Resolve the bounded dependent constexpr member-call overload set when
  exactly one fixed-arity method has the call's argument count and exact
  compatible integer parameter types. Regression covers a zero-argument and
  one-argument overload; same-arity overload ranking is not inferred.
- [x] Match GCC C++20, pass the focused overload gate, and rerun the complete
  native-Windows `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx` suite (exit 0), including i686/AMD64 generation and AMD64 host
  execution.
- [x] Rank same-arity overloads for one integer argument when every candidate
  has a fixed, by-value integer parameter. Use the existing implicit-conversion
  ranking and evaluate only a unique best candidate; the regression adds
  `value(short)` beside `value(int)` and confirms the exact `int` overload wins.
- [x] Match GCC C++20 and pass `test-cxx-function-template-overloads` plus the
  complete native-Windows `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests
  -j1 test-cxx` suite (exit 0), including i686/AMD64 generation and AMD64
  execution.
- [x] Commit the implementation and regression on local `main` as
  `5167167b7eb0d351416004e8f0c54beb9c247fea`; keep it local and do not push.
- [x] Extend same-arity ranking to multiple integer arguments using the
  per-argument conversion-rank dominance relation. The fixture compares
  `value(int, int)` against `value(short, int)` and `value(int, short)`; GCC and
  RCC select the unique exact-match candidate.
- [x] Pass `test-cxx-function-template-overloads` and the complete native-
  Windows `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1 test-cxx`
  suite (exit 0), including both target widths and AMD64 execution.
- [x] Commit the implementation and regression on local `main` as
  `c6bce80c101b6af55116ea8455142d255e4e0b59`; do not push.
- [x] Extend dependent constexpr member-call overload ranking to fixed,
  non-variadic public static constexpr integer-returning methods with
  by-value integer parameters and trailing integer default arguments, when at
  least one integer argument is explicit. Rank only supplied arguments using
  the existing per-argument conversion dominance; evaluate omitted defaults
  only on the uniquely selected candidate. Regression compares
  `value(short, int = 1)` with `value(int)` and verifies both fixed-pattern and
  dependent-call outcomes against GCC C++20.
- [x] Pass `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` (exit 0), including i686/AMD64
  generation, AMD64 host execution, GCC host execution, and ambiguity
  diagnostics; commit the implementation and regression as `213b352` on local
  `main`. Do not push.
- [x] Complete the full native-Windows `mingw32-make SHELL=cmd.exe CC=gcc
  TEST_OUT=build/tests -j1 test-cxx` gate for this change; it completed with
  exit status 0. The full output is retained in
  `build/tests/test-cxx-full-gate.log`.
- [x] Cover dependent constexpr member calls that provide zero explicit
  arguments when the unique viable public static constexpr integer-returning
  method supplies all arguments through integer defaults. The regression checks
  a defaulted call evaluating to `25` and the `value() == 1` ordering case,
  where GCC C++20 selects the dependent-call pattern over the matching fixed
  value pattern.
- [x] Pass the focused `test-cxx-function-template-overloads` gate and the full
  native-Windows `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx` aggregate (exit 0); output is retained in
  `build/tests/test-cxx-default-zero-gate.log`.
- [ ] Extend the bounded integer-only dependent member-call evaluator to
  variadic overloads, NTTP packs, arbitrary dependent call expressions,
  inherited/non-static receiver forms, and broader C++20 conversions and
  partial ordering.

## RCC continuation (2026-10-12, CI coverage re-audit)

- [x] Cross-check `.github/workflows/ci.yml` against `TEST_CI_TARGETS`,
  `C17_REGRESSION_TARGETS`, and `CXX_REGRESSION_TARGETS`. The production gate
  includes C17/C++20, `test-debug-info`, IR/MIR, verified backend, optimizer,
  ABI, format/golden/determinism/property/fuzz, and bootstrap targets; the
  independent workflow jobs cover GCC/Clang sanitizer suites and host smoke
  checks. `test-cxx20-unsupported-boundaries` is part of `test-cxx` and thus
  the production gate. This inventory does not upgrade the separately
  recorded incomplete Clang full-`test-ci` run to a pass.
- [x] Strengthen the unsupported C++20 modules/coroutines target: clear stale
  `.ro` outputs, require all four explicit module/coroutine diagnostics on both
  architectures, and assert that neither invalid translation produces an
  object.
- [x] Pass `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx20-unsupported-boundaries` on native Windows (exit 0).
- [x] Extend dependent static constexpr member-call evaluation to integer
  `const T&` arguments and integer `T&&` arguments. Preserve lvalue/rvalue
  viability while ranking overload candidates, and bind the referred scalar
  value in constexpr evaluation; GCC C++20 plus RCC i686/AMD64 execution cover
  both forms and fixed-value versus dependent-call selection.
- [x] Pass the focused native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` gate (exit 0), including GCC oracle,
  both target widths, and AMD64 execution.
- [x] Pass the full native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1 test-cxx`
  aggregate (exit 0); output is retained in
  `build/tests/test-cxx-default-zero-gate.log`.
- [x] Evaluate dependent integer calls to a member-function template when
  template arguments are deduced or explicitly supplied and materialization
  yields exactly one public static constexpr integer-returning specialization
  with supported integer constant arguments. Cover fixed-value versus
  dependent-call ordering for both forms against GCC C++20, with i686/AMD64
  output and AMD64 execution.
- [x] Pass the focused native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` gate and rerun the full
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1 test-cxx`
  aggregate (both exit 0); full output is retained in
  `build/tests/test-cxx-default-zero-gate.log`.
- [ ] Extend the unified ordinary/member-template overload set to variadic
  overloads, NTTP packs, non-integer arguments/conversions, inherited and
  non-static receiver forms, and the remaining C++20 partial-ordering rules.

## RCC continuation (2026-10-13, mixed dependent member-call overloads)

- [x] Combine materialized member-function-template candidates with same-name
  ordinary member overloads for bounded dependent constexpr integer calls.
  Compare supplied arguments using conversion-rank dominance and prefer an
  ordinary function over a member-template specialization when their parameter
  types are identical. Cover the GCC C++20 result and fixed/dependent NTTP
  ordering on both RCC targets, with AMD64 execution.
- [x] Pass the focused native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` gate and the full
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1 test-cxx`
  aggregate (both exit 0).
- [ ] Continue with variadic/NTTP-pack calls, non-integer conversions,
  non-integer explicit arguments or selected conversions, inherited and
  non-static receiver lookup, broader dependent expressions, and remaining
  C++20 overload-ranking rules.

## RCC continuation (2026-10-13, non-integer overload competitors)

- [x] Include viable non-integer parameter types in overload ranking for
  dependent constexpr calls whose explicit arguments are integer constants;
  retain the evaluator's integer-only restriction for the selected constexpr
  call. Verify exact-match integer versus floating-conversion selection against
  GCC C++20, with i686/AMD64 output and AMD64 execution.
- [x] Pass the focused native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` gate and the full
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1 test-cxx`
  aggregate (both exit 0).
- [ ] Continue with non-integer explicit arguments and selected conversions,
  variadic/NTTP-pack calls, multiple-base/using inherited lookup, non-static
  receiver lookup, broader dependent expressions, and remaining C++20
  overload-ranking rules.

## RCC continuation (2026-10-13, inherited static member calls)

- [x] Include inherited ordinary methods from the published `TypeMethod` set
  when ranking bounded dependent constexpr integer calls. Cover public static
  constexpr lookup through a derived class and fixed-value versus dependent-
  call selection against GCC C++20, with i686/AMD64 output and AMD64 execution.
- [x] Pass the focused native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` gate and the full
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1 test-cxx`
  aggregate (both exit 0).
- [x] Extend inherited template lookup through multiple bases when exactly one
  base contributes the name, and honor a `using Base::member` selection among
  competing bases. Verify nonzero receiver adjustment by reading inherited
  state; verify unselected competing templates diagnose ambiguity against GCC
  C++20, with i686/AMD64 generation and host execution.
- [x] Validate inherited non-static receiver adjustment for ordinary,
  virtual-base, and multiple-base paths; pass the focused and full C++ gates.
- [ ] Continue with non-integer explicit arguments/selected conversions,
  variadic/NTTP-pack calls, broader dependent expressions, and remaining C++20
  overload-ranking rules.

## RCC continuation (2026-10-13, inherited member-function templates)

- [x] Collect visible member-function templates through single-base
  inheritance when no derived declaration hides the name. Preserve effective
  access and rank their deduced or explicitly supplied specializations against
  inherited ordinary methods for bounded dependent constexpr integer calls.
  Check fixed-value versus dependent-call selection against GCC C++20, with
  i686/AMD64 output and AMD64 execution.
- [x] Pass the focused native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` gate and the full
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1 test-cxx`
  aggregate (both exit 0).
- [x] Extend inherited template lookup through multiple bases when exactly one
  base contributes the name, and honor a `using Base::member` selection among
  competing bases. Verify nonzero receiver adjustment by reading inherited
  state; verify unselected competing templates diagnose ambiguity against GCC
  C++20, with i686/AMD64 generation and host execution.
- [ ] Continue with non-integer explicit arguments/selected conversions,
  variadic/NTTP-pack calls, broader dependent expressions, and remaining C++20
  overload-ranking rules.

## RCC continuation (2026-10-13, inherited non-static member-template receivers)

- [x] Resolve ordinary deduced calls to inherited non-static member templates
  through public single-base inheritance. Register each specialization for code
  generation and adjust its implicit object argument for virtual-base paths.
  The regression reads an inherited base field; emitted virtual-base calls add
  4 bytes on i686 and 8 bytes on AMD64. Compare with GCC C++20 and execute both
  ordinary- and virtual-base-derived receiver cases on the host after
  i686/AMD64 generation.
- [x] Pass the focused native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` gate and full
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1 test-cxx`
  aggregate (both exit 0).
- [x] Extend inherited template lookup through multiple bases when exactly one
  base contributes the name, and honor a `using Base::member` selection among
  competing bases. Verify nonzero receiver adjustment by reading inherited
  state; verify unselected competing templates diagnose ambiguity against GCC
  C++20, with i686/AMD64 generation and host execution.
- [ ] Continue with non-integer explicit arguments/selected conversions,
  variadic/NTTP-pack calls, broader dependent expressions, and remaining C++20
  overload-ranking rules.

## RCC continuation (2026-10-13, arithmetic function return conversions)

- [x] Convert arithmetic return expressions to the declared function return
  type when either side is floating-point, in both i686 and AMD64 native
  codegen. Cover floating-to-integer returns from an explicitly instantiated
  inherited member-function template and integer-to-double returns.
- [x] Pass the focused native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` gate and the complete
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1 test-cxx`
  aggregate (both exit 0), including target generation, AMD64 execution, and
  GCC C++20 comparison.
- [ ] Continue broader non-integer explicit-argument deduction/conversion,
  variadic and NTTP-pack overloads, dependent expressions, and remaining C++20
  overload-ranking rules.

## RCC continuation (2026-10-13, floating arguments in dependent member calls)

- [x] Allow floating-point by-value parameters in bounded dependent constexpr
  member calls, including explicit `<double>` member-template specialization
  and integer-to-double argument conversion during dependent value evaluation.
  Compare fixed-value versus dependent-call overload selection against GCC
  C++20; generate i686/AMD64 output and execute the AMD64 host regression.
- [x] Pass the focused native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` gate and full
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1 test-cxx`
  aggregate (both exit 0).
- [ ] Continue with variadic/NTTP-pack dependent calls, wider argument
  conversions and expressions, and remaining C++20 overload-ranking rules.

## RCC continuation (2026-10-13, floating constexpr call arguments)

- [x] Accept integer and floating-point constant arguments in dependent
  constexpr member-call evaluation. Use semantic conversion ranking for
  floating overloads and the scalar constexpr evaluator to verify argument
  constants and the selected integer result. Cover an explicit `<double>`
  member-template call, `float`-to-`double` promotion against an `int`
  overload, and `float`-to-`int` conversion for the selected parameter;
  compare with GCC C++20 and execute generated i686/AMD64 code on the host.
- [x] Pass the focused native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` gate (including the added
  float-to-integer regression) and full
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1 test-cxx`
  aggregate (both exit 0; the full aggregate passed after the evaluator
  implementation and the focused gate was rerun after the added regression).
- [ ] Continue variadic/NTTP-pack dependent calls, broader arithmetic
  conversions and expressions, and remaining C++20 overload-ranking rules.

## RCC continuation (2026-10-13, dependent non-type pack member calls)

- [x] Expand a dependent `Head::template value<Values...>()` member-template
  call while deducing a non-type function-template pack from a class-type
  pack expansion. Preserve pending pack values through dependent type/value
  substitution and overload partial ordering. Cover non-empty and empty packs,
  compare against GCC C++20, generate i686/AMD64 code, and execute the
  AMD64-host regression.
- [x] Pass the focused native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` gate and full
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1 test-cxx`
  aggregate (both exit 0 after the empty-pack case was added).
- [ ] Continue broader variadic/NTTP-pack dependent calls, expressions, and
  remaining C++20 overload-ranking rules.

## RCC continuation (2026-10-13, parenthesized NTTP pack expressions)

- [x] Recognize parenthesized non-type expressions as explicit arguments to a
  dependent member-template call. Cover `Head::template value<(Values + 1)...>()`
  with both empty and non-empty packs, compare against GCC C++20, generate
  i686/AMD64 code, and execute the AMD64-host regression.
- [x] Pass the focused native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` gate (exit 0).
- [x] Pass the full native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1 test-cxx`
  aggregate after this parser change (exit 0).
- [ ] Continue broader expression-pack patterns, interactions between
  multiple expansions, and remaining C++20 overload-ranking rules.

## RCC continuation (2026-10-13, C-style variadic member overload ranking)

- [x] Parse raw C-style ellipses in C++ class-member and free-function
  parameter lists. Extend both bounded dependent constexpr member-call
  candidate evaluators to rank arguments beyond fixed parameters as ellipsis
  conversions. Cover variadic competitors alongside fixed ordinary methods and
  materialized member-function templates, plus free-function invocation;
  compare with GCC C++20 and run i686/AMD64 generation and AMD64 execution.
- [x] Pass the focused native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` gate and the full
  `mingw32-make SHELL=cmd.exe CC=gcc CXX=g++ TEST_OUT=build/tests -j1
  test-cxx` aggregate (both exit 0).
- [ ] Continue with NTTP packs, broader dependent expressions and conversions,
  inherited/non-static receiver cases, and remaining C++20 overload-ranking
  rules.

## RCC continuation (2026-10-13, C-style variadic function templates)

- [x] Allow function-template deduction to finish from fixed parameters when
  additional call arguments bind to a trailing C-style ellipsis. Accept the
  remaining arguments only when the source function type is variadic, retaining
  the too-many-arguments diagnostic for ordinary function templates. Cover a
  deduced `T` followed by multiple ellipsis arguments, compare with GCC C++20,
  and verify i686/AMD64 generation plus AMD64 execution.
- [x] Pass the focused native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` gate and the full
  `mingw32-make SHELL=cmd.exe CC=gcc CXX=g++ TEST_OUT=build/tests -j1
  test-cxx` aggregate (both exit 0 after the function-template regression).
- [x] Cover static class-member variadic function templates in dependent
  constexpr calls, including a fixed ordinary overload competitor and scalar
  ellipsis arguments subject to default argument promotions.
- [x] Complete mixed free-function ordinary/template overload coverage and
  non-scalar ellipsis ABI promotion coverage in the later continuation entries
  below, including GCC comparison and AMD64 execution.
- [ ] Continue remaining C++20 overload-ordering rules.

## RCC continuation (2026-10-13, variadic constexpr member templates)

- [x] Allow the dependent static-member-call evaluator to materialize and rank
  constexpr variadic member-function templates, compare them with ordinary
  fixed-parameter members, and evaluate constant integer/floating ellipsis
  arguments after default argument promotions.
- [x] Compare with GCC C++20 and run i686/AMD64 generation plus AMD64
  execution. Pass the focused native-Windows gate and the full `test-cxx`
  aggregate (exit 0) after the evaluator change; rerun the focused
  `test-cxx-function-template-overloads` gate after adding the floating
  ellipsis-argument regression (exit 0).
- [x] Extend constexpr-call coverage to non-scalar ellipsis arguments and
  generalize beyond the currently evaluated integer/floating subset.

## RCC continuation (2026-10-13, mixed free-function template overloads)

- [x] Keep the deduced function-template specialization in the call's
  overload set until sema compares it with ordinary same-name functions. Apply
  the non-template preference only when conversion sequences are
  indistinguishable; preserve a template when its conversion sequence is
  better.
- [x] Cover both fixed-arity and C-style variadic ordinary functions against
  function templates, including multiple ordinary overloads. In both single-
  and multi-candidate sets, the ordinary overload wins an identical `int`
  match, while the template wins for `char` against integral promotions and
  for a derived pointer where two unrelated base conversions are incomparable.
  Compare results with GCC C++20 and verify both target assembly paths plus
  AMD64 execution.
- [x] Pass the focused native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` gate (exit 0, including its GCC oracle).
- [x] Pass a mixed INTEGER/SSE trivial aggregate through ordinary and
  function-template C-style ellipsis overloads, retrieve it with
  `__builtin_va_arg`, compare GCC C++20 behavior, and execute the AMD64 host
  image after generating both target assemblies.
- [x] Cover non-scalar ellipsis ABI promotions and broader mixed
  ordinary/template overload sets in the continuation entries below.
- [ ] Continue remaining C++20 overload-ordering rules.

## RCC continuation (2026-10-13, free-function template ADL)

- [x] Include function templates from argument-associated namespaces in
  unqualified calls, traversing pointer/reference, base-class, and class-
  template argument associations while respecting inline-namespace lookup.
  Combine ADL ordinary functions with template candidates so conversion
  ranking can select an exact template over a worse ordinary conversion.
- [x] Clear ambiguity among inferior candidates when a later candidate has a
  strictly better conversion sequence. Cover a mixed namespace overload set
  with incomparable ordinary base conversions and an exact template match.
- [x] Pass the native-Windows focused function-template overload gate, the
  C++ language-core gate, the multiple-namespace ADL gate, the two-phase ADL
  gate, and the GCC C++20 oracle. Both target generations and AMD64 host
  execution pass.
- [x] Correct free-function template ABI mangling when deduced arguments also
  occur in the parameter signature. Encode type/value arguments, dependent
  return and parameter types, nested namespaces, and Itanium substitutions;
  compare emitted symbols against GCC for ordinary, defaulted, nested-alias,
  and mixed ADL-tie overloads on i686/AMD64. The mixed tie retains distinct
  ordinary/template symbols. `test-cxx-function-templates`,
  `test-cxx-function-template-overloads`, `test-cxx-non-type-templates`, and
  the complete `test-cxx` aggregate pass. RCC commit `f2c0f09`.
- [x] Cover non-scalar ellipsis ABI promotions in the subsequent variadic ABI
  continuation entries.
- [ ] Continue remaining C++20 overload-ordering rules.

## RCC continuation (2026-10-13, optimized inline debug identity and Clang bootstrap)

- [x] Match verified-backend inline debug-origin callers to cloned function
  owners by semantic identity when duplicate `Decl` nodes share one body:
  require function kind, shared body, matching linkage, compatible function
  types, and the same ABI link name. This retains nested optimized C++ inline
  child PC ranges without conflating distinct overloads.
- [x] Pass `test-debug-info` with GCC- and Clang-host RCC builds. Refresh only
  the C++20 core `.ro` golden hashes after deterministic double generation;
  all `.rin`, `.rll`, `.drv`, and other corpus hashes remain unchanged. The
  complete golden corpus passes with both host builds.
- [x] Include `<limits.h>` explicitly in `lexer.c`, which compares the
  character-literal code-unit count with `UINT_MAX`; this is required by the
  `-nostdinc` bootstrap include set. Complete `test-bootstrap-core`,
  `test-bootstrap-link`, `test-bootstrap-execute`, and `test-bootstrap-stage2`
  with Clang-built RCC on i686 and x86_64.
- [x] Complete the full native-Windows MSYS2 Clang 18.1.4 `test-ci` production
  gate on 2026-10-13 (exit 0), including C17/C++20, image-format validation,
  golden/fuzz/property, and WSL Ubuntu-24.04 stage1-to-stage2 bootstrap.
- [ ] Continue the broader incomplete DWARF/unwind, optimization, and C++20
  conformance work; this gate pass does not imply full language conformance.

## GCC production gate rerun (2026-10-13)

- [x] Re-run the complete native-Windows MinGW GCC 13.2.0 `test-ci` gate on
  frozen RCC revision `df4068f277363f3ac3a3af14fe939a5d0bbdc4ba`, using the
  existing `obj/windows-gcc` and `build/gcc-current` outputs. Exit 0: C17/C++20,
  AQC, IR/MIR, verified backend, optimizer, ABI, RIN/RLL/DRV validation,
  golden/determinism/property/fuzz, linking/relocations, and WSL Ubuntu-24.04
  stage1 execution plus byte-identical stage2 rebuilds for i686 and x86_64.
  The initial unprivileged attempt could not start WSL (`Wsl/Service/E_ACCESSDENIED`);
  the approved elevated rerun completed the same gate successfully. Clang's
  full `test-ci` pass is recorded in the preceding 2026-10-13 section.

## SysV 64-byte variadic aggregate arguments (2026-10-13)

- [x] Align the actual AMD64 caller stack pointer to the maximum alignment of
  outgoing MEMORY-class arguments, rather than aligning only offsets relative to
  a merely 16-byte-aligned frame. Preserve and restore the pre-call stack pointer
  across the callee while keeping outgoing aggregate payloads and following
  stack arguments at their ABI offsets.
- [x] Extend `test-cxx-variadic-aggregate` with an `alignas(64)` 64-byte
  aggregate after a spilled integer and before a trailing 24-byte aggregate.
  Assert the aggregate's size/alignment, generate i686 and AMD64 caller/callee
  objects, and execute GCC-caller/RCC-callee plus RCC-caller/GCC-callee in both
  directions at `-O0` and `-O2`. The focused gate and full native-Windows GCC
  `test-cxx` aggregate both pass (exit 0).
- [x] Re-run full GCC `test-ci` with this code-generation change present,
  including format validation and stage1-to-stage2 bootstrap. The complete gate
  passed on 2026-10-14; the tested content is now committed as `001924c`.

## RCC continuation (2026-10-14, verified fallback DWARF ranges)

- [x] Clear partially generated verified-backend AST statement ranges before
  returning to the classic backend, then rebuild its classic module from the
  clean ranges. This prevents mixed backend offsets from producing cross-
  function DWARF location ranges when a later C++ function requires fallback.
- [x] Add an i686/AMD64 C++ regression with an accepted prefix function and a
  later nontrivial conversion operator that deliberately leaves the typed SSA
  subset. Verify exactly one fallback per target, both function DIEs, and
  prefix-local location-list ownership; `test-debug-info` passes.
- [x] Grow the code-section buffer until it covers the full requested size.
  Large verified text copied into the DWARF/unwind metadata module previously
  exceeded a single doubling and corrupted the host heap. Permit the verified
  i686 assembly test adapter to validate and omit its non-code `.unwind`
  section. `test-verified-builtins` and the full `test-verified-backend`
  aggregate pass.
- [x] Pass `test-optimize` and `test-debug-info` on the native Windows GCC host.
  The full WSL bootstrap/self-host gate remains separately pending as noted
  above; no substitute stub was used.

## RCC continuation (2026-10-14, constexpr aggregate ellipsis arguments)

- [x] Extend dependent constexpr member-template calls to validate and
  materialize trivial struct/union arguments passed through `...`, while
  preserving default promotions for fixed arithmetic arguments. Non-trivial
  class ellipsis evaluation remains outside this constexpr subset.
- [x] Compare the fixture with GCC C++20; compile i686 and AMD64, and execute
  the AMD64 image. The regression uses a trivial mixed INTEGER/SSE aggregate
  in the ellipsis slot.
- [x] Pass native-Windows GCC
  `mingw32-make SHELL=cmd.exe CC=gcc CXX=g++ TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` and the complete `test-cxx` aggregate
  (both exit 0).

## RCC continuation (2026-10-14, unwind relocation and FDE width)

- [x] Teach TLS and direct-relocation image checks to account for the loadable
  `.unwind` section and its base relocations when the section overlaps the
  broad `.code` virtual range. Verify section bounds, relocation targets, and
  TLS relocation counts without weakening relocation type checks.
- [x] Zero-extend the 32-bit function code size before serializing an x86_64
  DWARF FDE address range. This removes a host-width shift undefined behavior
  that made GCC duplicate the low half while Clang emitted zero in the upper
  half. Add an explicit regression asserting the nonzero low word and zero
  upper word.
- [x] Regenerate all 10 x86_64 `.ro` golden hashes via deterministic
  double-generation; leave `.rin`, `.rll`, and `.drv` hashes unchanged. The
  complete golden corpus passes in check mode.
- [x] Pass the complete native-Windows MinGW GCC `test-ci` gate on this working
  revision (exit 0), including C17/C++20, IR/MIR, verified backend, optimizer,
  debug info, ABI, RIN/RLL/DRV validation, golden/determinism/property/fuzz,
  and Ubuntu-24.04 WSL stage1 execution plus byte-identical i686/x86_64 stage2
  rebuilds.

## RCC continuation (2026-10-14, dependent C++20 conditional explicit)

- [x] Retain dependent `explicit(bool)` conditions on C++ methods until class
  template arguments are substituted, then evaluate the condition for
  constructors and conversion operators. Evaluate dependent conversion
  function-template conditions after deduction.
- [x] Make invalid C++ copy-initialization from an explicit conversion a
  diagnostic error while preserving the existing C warning behavior.
- [x] Add GCC C++20 positive/negative oracle coverage for dependent class
  template constructors/conversion operators and conversion-function-template
  conditions. Compile i686 and AMD64, execute the AMD64 positive image, and
  reject invalid implicit constructor/conversion cases on both targets.
- [x] Pass `test-cxx-conditional-explicit` (exit 0). The complete native
  Windows `test-ci` run with Clang explicitly selected passed its Windows
  test targets through format validation, fuzz/property, link, and relocation
  checks, then hit the unprivileged WSL access boundary. The separately
  approved `test-bootstrap-stage2` run (exit 0) completed core, link, stage1
  execution, and reproducible i686/AMD64 stage2 gates.

## RCC continuation (2026-10-14, C++ variadic enum promotions)

- [x] Apply C++ default argument promotions to unscoped enum arguments in both
  i686 and AMD64 call lowering: narrower underlying types promote to `int`,
  while wider/fixed underlying types retain their ABI width. Preserve scoped
  enum types and leave C variadic behavior unchanged.
- [x] Cover signed/unsigned narrow fixed enums, fixed `int`/`unsigned int`,
  fixed signed/unsigned `long long`, scoped 32/64-bit enums, and an inferred
  unscoped enum whose value requires `unsigned int`.
- [x] Verify GCC/RCC variadic interoperability in both directions on AMD64,
  plus i686/AMD64 assembly and object generation. The focused
  `test-cxx-variadic-scalar-promotions` gate passes (exit 0).
- [x] Re-run the complete native-Windows MinGW GCC `test-cxx` aggregate after
  adding the inferred `unsigned int` enum case; the full aggregate exits 0.
- [x] Execute the interoperability fixture as native i686 Linux processes on
  the POSIX host: GCC caller with RCC callee and RCC caller with GCC callee.
  `test-cxx-variadic-scalar-promotions` now assembles and links both
  combinations on POSIX, then runs them; WSL passes in both directions. The
  production CI Ubuntu image installs the multilib toolchain, so this is also
  reached through `test-cxx`/`test-ci`. This does not claim RinOS target-runtime
  or physical-hardware execution.

## RCC continuation (2026-10-14, variadic aggregate register rollback)

- [x] Cover a two-INTEGER-eightbyte aggregate passed in the remaining AMD64
  SysV GP registers followed by a stack-spilled scalar.
- [x] Cover a reversed SSE/INTEGER aggregate when the GP bank is exhausted;
  verify whole-aggregate overflow does not consume the available SSE slot and
  that the following `double` is still retrieved from the register-save area.
- [x] Verify the complementary rollback cases: a two-INTEGER aggregate with
  only one GP slot left must leave that slot for the following scalar, and a
  reversed SSE/INTEGER aggregate with the SSE bank exhausted must leave its
  remaining GP slot for the following integer.
- [x] Cover a 9-byte `#pragma pack(1)` aggregate containing an unaligned
  `double`. Verify it is passed/retrieved through the MEMORY path and that the
  following scalar `double` still uses the available SSE register.
- [x] Pass the focused `test-cxx-variadic-aggregate` gate at `-O0` and `-O2`,
  including GCC/RCC caller-callee interop in both directions and i686/AMD64
  code generation.
- [x] Re-run the full native-Windows MinGW GCC `test-cxx` aggregate after the
  packed MEMORY-class case; it completed successfully (exit 0).
- [x] Cover a union whose overlapping `double` and integer members merge to
  the SysV INTEGER class. With one GP slot remaining, retrieve the union from
  that slot, the following integer from overflow, and the following `double`
  from the untouched SSE register bank; verify GCC/RCC interoperability.
- [x] Pass focused `test-cxx-variadic-aggregate` after the union-class
  regression at `-O0` and `-O2`, with GCC/RCC interop in both directions and
  i686/AMD64 code generation.
- [x] Run the full native-Windows MinGW GCC `test-cxx` after the union-class
  regression; it completed successfully (exit 0).
- [ ] Continue additional ABI classes/register-overflow combinations and
  remaining C++20 overload-ordering rules.

## RCC continuation (2026-10-14, disjunctive C++20 constraint subsumption)

- [x] Compare otherwise identical constrained function templates using `A`
  and `A || B`. When both constraints are satisfied, the `A` overload must
  win because its normalized constraint subsumes the disjunction. Compare with
  GCC C++20 and execute generated AMD64 code after i686/AMD64 generation.
- [x] Pass focused native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` (exit 0).
- [x] Re-run the full native-Windows MinGW GCC `test-cxx` aggregate after this
  regression; it completed successfully (exit 0).
- [ ] Continue constraint subsumption, dependent expressions, and the other
  open C++20 overload-ordering cases.

## RCC continuation (2026-10-14, equivalent C++20 constraint ambiguity)

- [x] Verify that `A` and `A || (A && B)` mutually subsume after DNF/CNF
  normalization and therefore do not order otherwise identical overloads.
  The call remains ambiguous, matching GCC C++20, on i686 and AMD64.
- [x] Pass focused native-Windows
  `mingw32-make SHELL=cmd.exe CC=gcc TEST_OUT=build/tests -j1
  test-cxx-function-template-overloads` (exit 0).
- [x] Also compare `A && (B || C)` with `(A && B) || (A && C)`; they mutually
  subsume despite different syntax, so GCC and RCC keep the call ambiguous on
  both targets. The focused overload gate passes with the new diagnostic.
- [x] Re-run the full native-Windows MinGW GCC `test-cxx` aggregate after this
  ambiguity regression; it completed successfully (exit 0).
- [x] Re-run full native-Windows MinGW GCC `test-cxx` after adding the
  distributive-normal-form case; it completed successfully (exit 0).
- [x] Check atomic-constraint identity through one shared concept expression
  mapped as `T*` versus `T`. Both constraints are satisfied, but their
  parameter mappings differ, so GCC and RCC must retain ambiguity on i686 and
  AMD64; the focused overload gate passes.
- [x] Re-run the full native-Windows MinGW GCC `test-cxx` after this distinct
  concept-mapping regression; it completed successfully (exit 0).
- [x] Extend the mapping check to a requires-expression atom (`*candidate`)
  instantiated with concept arguments `T*` and `T`; both requirements are
  satisfied, but the distinct mappings must remain ambiguous. GCC and RCC
  agree on i686 and AMD64, and the focused overload gate passes.
- [x] Re-run the full native-Windows MinGW GCC `test-cxx` after the
  requires-expression mapping case; it completed successfully (exit 0).
- [x] Check a non-type concept atom mapped from `N` versus `N + 1`. Both
  constraints are true for the selected argument, but distinct mappings keep
  the overload set ambiguous; GCC and RCC agree on i686 and AMD64, and the
  focused overload gate passes.
- [x] Re-run full native-Windows MinGW GCC `test-cxx` after the non-type
  concept-mapping case; it completed successfully (exit 0).
- [x] Compare a concept-defined `Value > 0` atom with the same expression
  written directly in a function-template requires-clause. Their source
  expressions differ, so the satisfied overloads remain ambiguous under GCC
  and RCC on both targets; the focused overload gate passes.
- [x] Re-run full native-Windows MinGW GCC `test-cxx` after the distinct
  atomic-source regression; it completed successfully (exit 0).
- [ ] Continue constraint subsumption and remaining C++20 overload ordering.

## RCC continuation (2026-10-14, compound type-pack partial ordering)

- [x] Compare `Box<Wrap<Types>...>` with the unconstrained `Box<Types...>`
  overload. Verify that the compound expansion wins for empty, single-element,
  and multi-element expansions, while a mixed wrapped/unwrapped pack selects
  the general overload. GCC C++20 agrees; the focused RCC overload gate passes
  i686/AMD64 code generation and AMD64 execution.
- [x] Compare `Box<Pair<Types, Types>...>` with `Box<Types...>`. Verify the
  repeated-type compound expansion wins for empty, single, and multi-element
  matching pairs, while differing pair members and mixed pair/plain elements
  select the general overload. GCC C++20 agrees; the full focused RCC overload
  gate passes i686/AMD64 code generation and AMD64 execution.
- [x] Combine dependent `sizeof...(Types)` and arithmetic
  `sizeof...(Types) + 1` class-template arguments with a compound trailing
  type-pack expansion and compare each against the corresponding direct pack.
  The compound expansion wins for empty, one-element, and multi-element
  wrapped packs; a mixed wrapped/plain pack selects the general overload. GCC
  C++20 agrees, and the full focused RCC overload gate passes on i686/AMD64
  with AMD64 execution. This also fixes empty-pack ordering, where RCC
  previously reported a false ambiguity because transformed patterns did not
  associate the count expression with their corresponding pack.
- [ ] Extend ordering to other dependent non-type expression forms,
  non-type/template-template packs, further compound/mixed pack sources, and
  their interactions with constraints and conversion-sequence ties.

## RCC continuation (2026-10-14, dependent alias-template scalar NTTP types)

- [x] Parse dependent declared types of the form
  `typename Owner::template value_type_template<short> Value` for scalar
  non-type template parameters in class and function templates.
- [x] Substitute the enclosing owner and alias-template arguments (including
  defaulted and explicit non-type alias arguments), resolve the inherited
  alias declaration, and evaluate its constraints when instantiating the
  declared parameter type.
- [x] Compare class and function specialization Itanium names byte-for-byte
  with GCC; cover negative values and verify concrete dependent declared types
  in the i686 and AMD64 debug-info gate.
- [x] Pass focused native-Windows MinGW GCC `test-debug-info` and
  `test-cxx-function-template-overloads` gates (exit 0).
- [x] Re-run and record the full native-Windows MinGW GCC `test-cxx` aggregate
  after the constrained-alias, template-template argument, and outer-parameter
  substitution, dependent-owner expression, alias type-pack, and dependent
  pack-forwarding changes; it completed successfully (exit 0).
- [x] Parse requires-clauses on nested alias templates and evaluate their
  constraints after dependent owner substitution. A satisfied `sizeof(Type)`
  constraint compiles; an unsatisfied `char` case matches GCC's rejection on
  i686 and AMD64.
- [x] Accept class-template template arguments in dependent alias-template
  specializations, including defaulted template-template parameters. Compare
  specialization symbols with GCC and verify mismatched signatures are
  rejected on both targets; also cover ordinary alias-template specialization.
- [x] Substitute a dependent alias-template template argument passed through
  an outer template-template parameter and preserve the complete final class
  specialization mangling; compare the emitted consumer symbol exactly with
  GCC on i686 and AMD64 in `test-debug-info`.
- [x] Resolve an alias declared through a dependent class-template owner whose
  own alias type refers to a second dependent owner alias; compare the exact
  consumer specialization symbol with GCC on i686 and AMD64.
- [x] Preserve and substitute the owner type expression when an alias resolves
  to another still-dependent alias; verify a three-stage owner/alias chain's
  exact specialization symbol against GCC on i686 and AMD64.
- [x] Support trailing type parameter packs in alias templates, including
  empty and multi-type packs expanded into a class-template-id; run both target
  objects and AMD64 execution in `test-cxx-alias-templates`. Keep
  template-template alias packs explicitly diagnosed; integral and enum
  non-type alias packs are tracked below.
- [x] Forward a dependent trailing type pack through one and two alias layers,
  including empty and multi-type packs plus fixed arguments around the forwarded
  expansion; verify both target objects and AMD64 execution.
- [x] Pass the focused native-Windows MinGW GCC `test-cxx-alias-templates`
  gate (exit 0).
- [x] Pass the full native-Windows MinGW GCC `test-cxx` aggregate after alias
  pack forwarding (exit 0).
- [ ] Extend dependent alias-pack forwarding to constrained alias templates
  and multiple distinct source packs; implement template-template alias packs,
  deeper owner/alias chains,
  pointer/reference/member-pointer declared types with non-integral NTTP
  values, and broader type-directed grammar.

## RCC continuation (2026-10-14, integral and enum alias value packs)

- [x] Parse trailing integral and enum non-type parameter packs in alias
  templates; accept explicit values, empty packs, and dependent pack
  expansions, while retaining a precise diagnostic for unsupported trailing
  template-template packs.
- [x] Forward dependent non-type packs through one and two alias layers,
  substitute transformed expressions such as `(Values + 1)...`, and preserve
  pending value-expression state through class/function template
  instantiation.
- [x] Cover empty and multi-element integral packs, enum-valued packs,
  expression transformations, class-template fields, function-template local
  types, and negative template-template-pack diagnostics in the alias tests.
- [x] Pass the focused native-Windows MinGW GCC
  `test-cxx-alias-templates` gate (exit 0), including the C++11 compatibility
  path.
- [x] Compile and run the alias fixture with GCC C++20 using
  `-Wall -Wextra -Werror` (exit 0).
- [x] Pass the full native-Windows MinGW GCC `test-debug-info` aggregate after
  these changes (exit 0).
- [x] Pass the full native-Windows MinGW GCC `test-cxx` aggregate after these
  changes (exit 0).
- [ ] Extend alias value-pack support to multiple distinct source packs,
  template-template packs, and non-integral structural NTTP types.

## RCC continuation (2026-10-14, constrained alias value packs)

- [x] Evaluate a C++20 alias-template requires-clause over a trailing integral
  value pack's `sizeof...(Values)` while preserving its pending pack context;
  accept a non-empty pack and reject the empty specialization.
- [x] Add a runnable positive fixture and negative empty-pack fixture; compare
  RCC and GCC diagnostics, require exactly one RCC error and no output object,
  and pass i686/AMD64 code-generation plus AMD64 execution in the focused alias
  gate. GCC C++20 `-Wall -Wextra -Werror` positive build/run also passes.
- [x] Pass the full native-Windows MinGW GCC `test-cxx` aggregate after the
  constrained alias-pack regression.
- [ ] Extend constrained aliases to multiple distinct source packs;
  template-template packs, wider transformations, and non-integral structural
  NTTP packs remain open.

## RCC continuation (2026-10-14, constrained alias value-pack folds)

- [x] Parse unary `&&` and `||` fold expressions directly as requires-clauses
  instead of consuming their parentheses as an optional clause wrapper.
  Evaluate per-value predicates for trailing integral alias packs; verify the
  empty `&&` fold identity, accepted/rejected non-empty packs, and GCC parity.
- [x] Pass `test-cxx-alias-templates` on i686/AMD64 with AMD64 execution and
  exact single-error/no-object checks for rejected constraints.
- [x] Pass the full native-Windows MinGW GCC `test-cxx` and `test-debug-info`
  aggregates after the parser change (both exit 0).
- [ ] Extend constrained alias packs to multiple distinct source packs,
  template-template packs, wider transformed expression trees, and
  non-integral structural NTTP types.

## RCC continuation (2026-10-14, nested arithmetic alias value-pack expansions)

- [x] Extend alias-pack transformation coverage from `Values + 1` to
  `Values * 3 + 2` and `((Values + 2) * 3 - 1)`. Both must instantiate the
  exact `AliasTransformedValues<8, 14>` type; verify through typed pointer
  calls and AMD64 execution.
- [x] Pass `test-cxx-alias-templates` on i686/AMD64 with AMD64 execution and
  GCC C++20 `-Wall -Wextra -Werror` compile/run.
- [x] Combine a per-value `&&` constraint fold with an arithmetic alias-pack
  transformation; accept positive values and reject a pack containing zero,
  with GCC parity on i686/AMD64 and AMD64 execution.
- [ ] Cover deeper dependent-owner chains and remaining transformed-expression
  operator combinations.

## RCC continuation (2026-10-14, bitwise and conditional alias pack transforms)

- [x] Verify transformed trailing alias-pack values using left shift,
  bitwise OR, remainder, comparison, and the conditional operator. Type-check
  exact pack values against GCC and execute the AMD64 fixture.
- [x] Pass `test-cxx-alias-templates` on i686/AMD64 with AMD64 execution and
  GCC C++20 `-Wall -Wextra -Werror` compile/run.
- [ ] Extend expression coverage through deeper dependent-owner chains and
  remaining operator combinations.

## RCC continuation (2026-10-14, dependent owner with constrained alias packs)

- [x] Substitute an enclosing class NTTP into both a nested alias pack's
  requires-clause and its transformed pack expansion. Accept owner value 2,
  reject owner value 0 with a non-empty positive value pack, and compare RCC
  diagnostics/results with GCC.
- [x] Pass `test-cxx-alias-templates` on i686/AMD64 with AMD64 execution,
  single-error/no-object negative checks, and GCC C++20 warning-clean run.
- [ ] Extend this coverage through deeper dependent-owner/alias chains and
  other source-pack combinations.

## RCC continuation (2026-10-14, zipped nested alias source packs)

- [x] Expand a dependent nested alias pattern using both the enclosing
  class's non-type pack and the alias's non-type pack, pairing corresponding
  values and rejecting mismatched pack lengths without producing an object.
- [x] Disambiguate a trailing nested-template `>>` from a shift operator while
  preserving a parenthesized non-type right-shift expression; compare positive
  and mismatched-pack behavior with GCC.
- [x] Pass the focused alias-template gate on i686/AMD64, including AMD64
  execution and GCC C++20 `-Wall -Wextra -Werror` compile/run.
- [x] Pass the full native-Windows MinGW GCC `test-cxx` and `test-debug-info`
  aggregates after these parser/substitution changes (both exit 0).
- [x] Commit the implementation, regression fixtures, and this TODO update on
  RCC `main`; update the parent repository's RCC gitlink to that commit. No
  push was performed.
- [ ] Extend multiple-source-pack expansion coverage to additional pack kinds
  and deeper dependent-owner/alias chains; template-template packs and
  non-integral structural NTTP packs remain open.

## RCC continuation (2026-10-14, mixed nested alias type/value packs)

- [x] Zip the enclosing and alias packs for type/type expansions and both
  mixed type/value directions, preserving the existing value/value behavior.
- [x] Reject unequal enclosing/alias type-pack lengths with a direct
  diagnostic and no output object; compare GCC C++20 behavior for mismatched
  type and value packs.
- [x] Pass the focused alias-template gate on i686/AMD64, including AMD64
  execution and GCC C++20 `-Wall -Wextra -Werror` compile/run.
- [x] Pass the full native-Windows MinGW GCC `test-cxx` and `test-debug-info`
  aggregates after the substitution change (both exit 0).
- [x] Commit the implementation, regressions, and TODO update on RCC `main`,
  and update the parent repository's RCC gitlink to that commit. No push.
- [ ] Extend to structural non-integral NTTP packs, deeper dependent-owner/
  alias chains, and additional cross-kind pack combinations.

## RCC continuation (2026-10-14, alias template-template packs)

- [x] Support trailing alias-template template-template packs, including a
  direct alias expansion and a nested alias expansion zipped with an enclosing
  type pack.
- [x] Check concrete and dependent template-template argument signatures;
  reject mismatches with one diagnostic and no output object.
- [x] Pass the focused `test-cxx-alias-templates` gate on i686/AMD64, including
  AMD64 execution, GCC C++20 `-Wall -Wextra -Werror`, and negative GCC parity.
- [x] Pass the full native-Windows MinGW GCC `test-cxx` aggregate after the
  final dependent-signature validation change (exit 0).
- [x] Pass the full native-Windows MinGW GCC `test-debug-info` aggregate after
  the final dependent-signature validation change (exit 0).
- [x] Add representative structural non-integral NTTP packs to nested-owner
  and nested-member-specialization coverage, including class-type, enum,
  nullptr, array, and floating representations. The structural owner-pack and
  owner-partial targets pass GCC symbol checks and i686/AMD64 generation and
  runtime.
- [ ] Continue additional and deeper dependent-owner/cross-kind pack
  combinations.

## RCC continuation (2026-10-14, nested template-template signatures)

- [x] Parse nested template-template parameter declarations recursively,
  retaining nested type, integral/dependent non-type, template-template, and
  pack signatures for compatibility checks.
- [x] Accept matching nested signatures and reject a mismatched nested
  signature with exactly one diagnostic and no output object; compare the
  negative case with GCC.
- [x] Pass the focused nested-signature tests and the full native-Windows
  MinGW GCC `test-cxx` and `test-debug-info` aggregates (both exit 0).
- [x] Accept alias templates as concrete and dependent template-template pack
  arguments, including defaults, type and non-type signatures, nested aliases,
  mixed alias/class packs, and invalid-signature diagnostics. The focused
  `test-cxx-alias-templates` gate passed for i686/x86_64, with x86_64 runtime,
  GCC C++20 runtime, and negative GCC parity.
- [ ] Expand deeper dependent-owner/cross-kind pack coverage and add defaults
  or constraints to nested template-template signatures. Representative
  structural non-integral owner packs are covered by the dedicated owner
  structural-value-pack target.

## RCC continuation (2026-10-14, alias-template owners of nested aliases)

- [x] Resolve an alias-template specialization used as the owner of a nested
  alias template, then validate its template-template pack arguments. Cover
  type and integral non-type owner parameters, and class/alias template actuals.
- [x] Pass the focused native-Windows `test-cxx-alias-templates` gate on
  i686/x86_64, including x86_64 execution and GCC C++20
  `-Wall -Wextra -Werror` execution.
- [x] Pass the full native-Windows MinGW GCC `test-cxx` and `test-debug-info`
  aggregates after this parser change (both exit 0).
- [x] Forward dependent template-template packs through nested alias-template
  specializations, zip them with owner type packs, and reject mismatched pack
  lengths. The focused `test-cxx-alias-templates` target passes on i686/AMD64
  with GCC parity; the implementation continuation records full C++ and
  debug-info aggregate passes.
- [ ] Extend deeper multi-source-pack coverage to remaining cross-kind
  dependent-owner patterns.
- [x] Add explicit/partial nested member specializations under dependent and
  outer class-template-specialization owners, including representative
  structural class-type, enum, pointer/nullptr, array, and packed NTTP patterns.
  The dedicated owner-partial and structural-value-pack targets pass exact GCC
  Itanium symbol checks, i686/AMD64 generation, and GCC/RCC host execution.
- [ ] Extend deeper cross-kind dependent-owner combinations and defaults or
  constraints in nested template-template signatures.
- [ ] Add further structural NTTP combinations specifically inside nested
  template-template signatures and implement defaults/constraints there.
- [x] Commit the parser implementation, regression fixture, and test record
  on RCC `main` as `cfa1568fb5c2083612a1b973c6e16022af9eba19`; no push.

## RCC continuation (2026-10-14, forwarded dependent template-template packs)

- [x] Forward a dependent template-template pack through a dependent nested
  alias-template specialization, zipping its templates with the owner class's
  type pack and preserving each corresponding owner argument.
- [x] Add a positive exact-type/runtime regression and a pack-length mismatch
  regression. Confirm GCC C++20 rejects the mismatch and RCC emits its direct
  diagnostic without an object file.
- [x] Pass the focused native-Windows `test-cxx-alias-templates` gate on
  i686/x86_64, including x86_64 execution, GCC C++20 `-Wall -Wextra -Werror`
  execution, and invalid-case parity checks.
- [x] Complete the full native-Windows MinGW GCC `test-cxx` aggregate after
  this change (exit 0; `build/test-cxx-20261014.log`).
- [x] Complete the full native-Windows MinGW GCC `test-debug-info` aggregate
  after this change (exit 0; `build/test-debug-info-20261014.log`).
- [ ] Continue with `Outer<T>::Inner<U>` nested class-template owners,
  structural non-integral NTTP packs, and deeper/cross-kind dependent-owner
  expansions. Defaults and constraints in nested template-template signatures
  also remain open.
- [x] Commit the parser change, regression fixtures, and this TODO record on
  RCC `main` as `9df04fa`, and its follow-up TODO commit as `60cc591`; no push.

## RCC continuation (2026-10-14, nested class templates on concrete owners)

- [x] Parse and register class-scope nested class-template declarations,
  preserve access, and diagnose duplicate names against nested aliases.
- [x] Resolve and specialize `Owner::Inner<T>` for a concrete non-template
  owner, including optional `typename`; enforce private/protected access from
  unrelated and derived class contexts.
- [x] Preserve the lexical owner in nested-class member Itanium names. Check
  the exact GCC symbol on i686/AMD64 assembly, generate PE-i386 objects, and
  execute compiler- and GCC-produced AMD64 programs.
- [x] Pass `test-cxx-nested-class-templates`, class-template methods,
  specialization, dependent-base regressions, `test-cxx-alias-templates`, and
  the full `test-debug-info` aggregate on native Windows (`cmd.exe`).
- [x] Re-run the complete native-Windows MinGW GCC `test-cxx` aggregate after
  this parser change (exit 0).
- [x] Support dependent owners before the enclosing class template is
  instantiated, including a multi-level `Outer<T>::Inner<U>::Leaf<V>` chain;
  owner specialization identity and nested access are covered by regressions.
- [x] Add nested member specializations owned by outer class-template
  specializations, including dependent owner declarations and structural NTTP
  owner arguments. The owner-partial and structural-value-pack target pair
  passes exact GCC symbol comparison, both target widths, and runtime checks;
  deeper cross-kind combinations remain tracked separately below.

## RCC continuation (2026-10-14, nested templates on concrete template owners)

- [x] Resolve `Outer<int>::Inner<long>` for a concrete class-template owner;
  substitute outer parameters into nested fields and methods, separate cached
  inner instances by owner specialization, and preserve the full owner in
  Itanium mangling.
- [x] Compare both owner-specialization symbols with GCC and pass the focused
  i686/AMD64 assembly, PE-i386 object, AMD64 runtime, nested-alias/access, and
  full `test-debug-info` aggregate (exit 0).
- [x] Enforce private versus protected nested-template access: reject private
  lookup from a derived class and accept protected lookup; verify invalid cases
  against GCC and ensure no object is emitted.
- [x] Resolve dependent class-template owners for `typename
  Outer<T>::template Inner<U>` while `T` is still dependent. Retain the owner
  and nested arguments until substitution, then select and instantiate the
  nested declaration belonging to the concrete owner. Cover class-template
  aliases and cloned function-template bodies, preserve per-owner
  specialization/mangling, and reject private nested templates after owner
  substitution.
- [ ] Extend nested member specialization matching to dependent and outer
  class-template-specialization owners; structural NTTP owner packs remain
  open.
- [x] Pass the complete native-Windows MinGW GCC `test-cxx` aggregate after
  this parser change (exit 0).

## RCC continuation (2026-10-15, dependent nested class-template owners)

- [x] Preserve a dependent owner separately from a nested class-template's
  own type/value arguments in `Outer<T>::template Inner<U>`, then substitute
  the owner before looking up and instantiating its nested declaration.
- [x] Exercise the owner-aware substitution in both class-template aliases
  and cloned function-template bodies. Check two distinct outer/inner type
  combinations, GCC Itanium symbols on i686/AMD64, PE-i386 object generation,
  AMD64 execution, and GCC C++20 `-Wall -Wextra -Werror` execution.
- [x] Reject a private nested class template after dependent-owner
  substitution; verify diagnostics and absence of an output object on both
  target architectures, with GCC also rejecting the fixture.
- [x] Pass the full native-Windows MinGW GCC `test-cxx` aggregate after this
  change (exit 0).
- [x] Pass the full native-Windows MinGW GCC `test-debug-info` aggregate after
  this change (exit 0).
- [x] Add explicit/partial nested member specializations for dependent or outer
  class-template-specialization owners, plus representative structural NTTP
  owner packs. Focused owner-partial and structural-value-pack gates pass GCC
  ABI comparison, i686/AMD64 generation, and runtime.

## RCC continuation (2026-10-15, dependent nested class-template owner chains)

- [x] Continue resolving each nested class-template-id in a dependent chain,
  retaining the newly specialized type as the next owner for
  `Outer<T>::template Inner<U>::template Leaf<V>`.
- [x] Add a focused chain regression for two distinct owner specializations;
  verify i686/AMD64 output, PE-i386 generation, AMD64 and GCC C++20 host
  execution, exact GCC Itanium symbol parity, and private nested-template
  rejection with no output object.
- [x] Capture enclosing type-template parameters through multiple lexical
  nested-class-template scopes, substituting the concrete outer and
  intermediate owner arguments in deeper bodies. The owner-chain fixture now
  reads all three levels and passes the full C++ and debug-info aggregates.
- [x] Substitute an enclosing integral non-type parameter through a deeper
  nested owner chain into a member-function body; verify GCC C++20, RCC i686
  and AMD64 compilation, and i686 freestanding plus AMD64 host execution.
- [x] Capture enclosing template-template parameters in direct and deeper
  nested-class owner chains, including an `Inner` member and a third-level
  `Leaf`. GCC C++20 syntax, RCC i686/AMD64 compilation, and the focused
  `test-cxx-class-template-dependent-nested-type` gate pass.
- [x] Expand enclosing type packs and integral non-type packs into trailing
  arguments of dependent class-template-ids in both `Inner` and `Leaf` owner
  scopes. Verify empty and three-element packs by the resulting payload size;
  GCC C++20 syntax and the focused dual-target runtime gate pass.
- [x] Substitute arithmetic-transformed integral pack expressions such as
  `OuterValues + 1` through both `Inner` and `Leaf`; verify the transformed
  values affect the resulting payload size for empty and three-element packs.
- [x] Expand outer type and inner integral non-type packs together in one
  dependent class-template-id pattern. Store the outer type in each element so
  both packs affect layout; verify the empty (1-byte) and two-element (8-byte)
  payloads plus an explicit 8-byte baseline for the nested `Inner` owner.
- [x] Expand a nested class-template fold across a local integral pack and
  enclosing type or integral non-type packs. Check matching two-element and
  empty `&&` folds at runtime; reject unequal pack lengths with one targeted
  diagnostic on i686 and AMD64.
- [x] Expand two enclosing integral packs as arguments to a call expression
  inside a nested class-template fold; verify GCC C++20, RCC i686/AMD64 output,
  i686 freestanding execution, and AMD64 host execution.
- [x] Expand an enclosing template-template pack in a nested class-template
  fold together with a local integral pack. Verify matching two-element and
  empty folds at runtime; reject mismatched lengths with one targeted
  diagnostic on i686 and AMD64, with GCC C++20 parity.
- [x] Pass the focused `test-cxx-class-template-dependent-nested-type` gate
  after adding call-pattern parsing and template-template pack sources.
- [x] Re-run the complete native-Windows MinGW GCC `test-cxx` aggregate after
  this parser change (exit 0).
- [x] Re-run the complete native-Windows MinGW GCC `test-debug-info` aggregate
  after this parser change (exit 0).
- [x] Expand two owner packs declared in distinct enclosing class-template
  scopes in one call-pattern fold. Verify matching two-element and empty cases
  at runtime, plus GCC parity and a single targeted i686/AMD64 mismatch
  diagnostic with no output object.
- [x] Re-run the focused nested-owner gate and the complete native-Windows
  MinGW GCC `test-cxx` and `test-debug-info` aggregates after adding the
  multiple-owner-scope regression (all exit 0).
- [x] Pass the complete native-Windows MinGW GCC `test-cxx` and
  `test-debug-info` aggregates after this parser change.
- [x] Register and select partial `Owner::Inner<T*>` and explicit
  `Owner::Inner<char>` class-template specializations for concrete non-template
  owners. Preserve the lexical owner in all specialization Itanium names.
- [x] Compare primary, partial, and explicit-specialization symbols with GCC
  on i686/AMD64; execute the RCC-generated AMD64 program and the GCC C++20
  program; pass `test-cxx-nested-class-templates` and the full `test-cxx` plus
  `test-debug-info` aggregates (exit 0).
- [x] Parse and select an explicit member class-template specialization on a
  concrete class-template owner (`template<> template<> struct
  Outer<int>::Inner<char>`). Keep the specialization owner-specific so
  `Outer<long>::Inner<char>` still selects the primary. Verify exact GCC
  Itanium symbols on i686/AMD64 and execute both RCC- and GCC-generated host
  programs.
- [x] Pass `test-cxx-nested-class-templates`, the complete native-Windows
  MinGW GCC `test-cxx` aggregate, and `test-debug-info` after the owner-aware
  specialization-selection change (all exit 0).
- [x] Parse and select `Outer<T>::Inner<U*>` partial specializations for a
  primary class-template owner. Preserve and match the dependent owner pattern
  separately from inner arguments; verify primary and partial instantiations
  across multiple owner types. Match empty and multi-element type, integral
  value, and template-template owner packs.
- [x] Resolve `Outer<T*>::Inner<U*>` against the nested template declared by
  the matching outer class-template partial specialization. Preserve that
  outer specialization's parameter environment for the member body while
  retaining primary-template identity for owner matching and mangling.
- [x] Compare GCC Itanium symbols for primary, partial, and owner-pack cases on
  i686/AMD64; execute RCC-generated AMD64 and GCC C++20 programs. Pass the
  focused nested owner-partial target and the full `test-cxx` and
  `test-debug-info` aggregates on native Windows (all exit 0).
- [x] Match outer partial owners with dependent integral non-type and
  template-template arguments. Select the nested template declared by the
  matching outer partial, carry the selected template-template argument into
  the primary-template identity, and preserve GCC Itanium names. The fixture
  checks runtime values, `sizeof` substitution, PE-i386 generation, and exact
  i686/AMD64 symbols; focused, full `test-cxx`, and `test-debug-info` gates
  pass.
- [x] Match a nested class-template specialization whose outer partial owner
  uses an explicitly declared `decltype(nullptr)` non-type argument. Preserve
  nullptr's distinct Itanium `LDnE` encoding, and verify GCC/RCC symbol
  equality for i686/AMD64 plus generated AMD64 runtime behavior.
- [x] Match a nested member partial specialization whose outer owner contains
  the dependent NTTP expression `OwnerValue * 3 + 2`. Retain and use the
  owner-head parameter context for both the `OwnerType*` pattern and value
  expression, independent of the primary's `OuterType`/`FirstValue` names.
  Verify a matching owner selects the partial, a nonmatching owner keeps the
  primary, exact GCC/RCC i686/AMD64 symbols, and AMD64 runtime behavior.
- [x] Re-run the focused nested-owner gate and full native-Windows MinGW GCC
  `test-cxx` and `test-debug-info` aggregates after dependent-expression and
  type-parameter-context owner matching; all completed with exit 0.
- [x] Extend the structural NTTP subset from one integral/enum field to
  multiple public integral/enum fields with a bounded 64-bit representation,
  declaration-order member decoding, and multi-literal Itanium mangling.
- [x] Nested structural subobjects containing public, non-bit-field integer or
  enum leaves are supported within the bounded 64-bit representation; a sole
  null ordinary-pointer leaf is also supported when nested.
- [ ] Non-null pointer-valued members and remaining standard-valid explicit
  member-specialization forms remain open.

## RCC continuation (2026-10-16, concrete-owner nested explicit specialization)

- [x] Cover explicit specialization of a nested class-template member owned by
  a non-template concrete class. Verify its GCC Itanium symbol against RCC on
  i686 and AMD64 and execute both generated AMD64 and GCC-host programs; the
  focused owner gate and full native-Windows MinGW `test-cxx` aggregate pass.
- [ ] Other standard-valid member-specialization forms and non-null structural
  pointer-valued NTTP members remain open.

## RCC continuation (2026-10-15, explicit nested specialization on a partial owner)

- [x] Explicitly specialize a nested class-template member for
      `Outer<int*>::Inner<char>` when `Outer<T*>` is an outer partial
      specialization. Keep the primary outer and outer-partial member cases
      distinct, run generated AMD64 and GCC programs, and compare exact
      Itanium symbols for all three selections on i686/AMD64.
- [ ] Other standard-valid nested member-specialization forms and non-null
      structural pointer NTTP values remain open.

## RCC continuation (2026-10-15, enum structural NTTP nested owners)

- [x] Extend the bounded structural owner argument representation to classes
  with exactly one public enum data member, including enum-valued owner packs
  and nested member reads from the class template parameter object.
- [x] Verify GCC/RCC Itanium symbols on i686 and AMD64 and generated-code plus
  GCC-host runtime behavior. The focused owner gate and full native-Windows
  `test-cxx` and `test-debug-info` aggregates all pass (exit 0).
- [x] Commit implementation, fixtures, and target checks as `0e32d07` on local
  RCC `main`; no push.
- [x] Add support for multiple public integral/enum data members in a
  structural class owner value.
- [x] Nested structural subobjects containing public, non-bit-field integer or
  enum leaves are supported within the bounded 64-bit representation; a sole
  null ordinary-pointer leaf is also supported when nested.
- [ ] Non-null pointer-valued members, additional dependent owner-head
  patterns, and remaining standard-valid explicit member specializations on
  these owner shapes remain open.

## RCC continuation (2026-10-15, null pointer structural NTTP members)

- [x] Accept structural classes with exactly one public ordinary pointer data
  member when its value is null, including `nullptr` and integer null-pointer
  constant initialization; retain the existing integral/enum cases and exclude
  member pointers from this bounded representation.
- [x] Omit the null pointer member literal in Itanium structural-value
  mangling, matching GCC; cover equal null values in owner packs, nested
  partial-specialization selection, and reads from a class template parameter
  object.
- [x] Verify exact GCC/RCC symbols on i686 and AMD64 and execute generated
  AMD64 and GCC-host programs. Focused owner regression plus full native
  Windows MinGW `test-cxx` and `test-debug-info` aggregates pass (exit 0).
- [x] Add support for multiple public integral/enum data members in a
  structural class owner value.
- [x] Nested structural subobjects containing public, non-bit-field integer or
  enum leaves are supported within the bounded 64-bit representation; a sole
  null ordinary-pointer leaf is also supported when nested.
- [ ] Non-null pointer-valued members and remaining standard-valid explicit
  member specializations on these owner shapes remain open.

## RCC continuation (2026-10-15, reordered structural NTTP owner heads)

- [x] Match a nested member partial specialization whose owner head reorders
  both parameter positions and kinds (`typename` and a one-field structural
  integral NTTP). Map owner-head parameters by identity, not by position, while
  retaining primary-owner fallback and member-value substitution.
- [x] Verify primary and partial selection, exact GCC Itanium symbols on
  i686/AMD64, and generated-code plus GCC-host runtime behavior. The focused
  `test-cxx-nested-class-template-owner-structural-value-pack`, full native
  Windows MinGW `test-cxx`, and `test-debug-info` gates all pass (exit 0).
- [x] Multi-member public integral/enum structural values are supported
  within the bounded flat 64-bit representation.
- [x] Nested structural subobjects containing public, non-bit-field integer or
  enum leaves are supported within the bounded 64-bit representation; a sole
  null ordinary-pointer leaf is also supported when nested.
- [ ] Non-null pointer-valued members and remaining standard-valid explicit
  member specializations remain open.

## RCC continuation (2026-10-15, empty structural NTTP values)

- [x] Accept an empty literal structural class as a class non-type template
  argument, including its empty-brace value-initialization representation;
  preserve the existing bounded one-public-member integral/enum/null-pointer
  subset.
- [x] Verify exact GCC/RCC Itanium symbols on i686 and AMD64, generated
  AMD64 runtime behavior, and GCC-host behavior. The focused structural-owner
  target and full native-Windows MinGW `test-cxx` plus `test-debug-info`
  aggregates all pass (exit 0).
- [x] Commit the RCC implementation, regression fixture, and target checks as
  `5443464` on local RCC `main`; no push.
- [x] Multi-member public integral/enum structural values are supported
  within the bounded flat 64-bit representation.
- [x] Nested structural subobjects containing public, non-bit-field integer or
  enum leaves are supported within the bounded 64-bit representation; a sole
  null ordinary-pointer leaf is also supported when nested.
- [ ] Non-null pointer-valued members and remaining standard-valid explicit
  member-specialization forms remain open.

## RCC continuation (2026-10-15, multi-member structural NTTP values)

- [x] Extend structural class NTTP values to multiple public, non-bit-field
  integer/enum data members, packing fields in declaration order into a
  bounded 64-bit representation. Decode signed fields for parameter-object
  member reads and emit one Itanium literal per non-pointer field.
- [x] Verify `{1, 2}` and `{-3, 4}` values, signed field decoding, exact
  GCC/RCC Itanium symbols, i686/AMD64 output, and generated AMD64 plus GCC-host
  runtime behavior. The focused owner target exits 0. The full native-Windows
  MinGW `test-cxx` and `test-debug-info` runs reached their completion markers
  without make errors.
- [x] Commit implementation, fixture, and target checks as `afd7a9b` on local
  RCC `main`; no push.
- [x] Nested structural subobjects containing public, non-bit-field integer or
  enum leaves are supported within the bounded 64-bit representation; a sole
  null ordinary-pointer leaf is also supported when nested.
- [ ] Non-null pointer-valued members and remaining standard-valid explicit
  member-specialization forms remain open.

## RCC continuation (2026-10-15, nested structural NTTP values)

- [x] Recursively pack nested public, non-bit-field integer/enum data members
  in declaration order within the bounded 64-bit value. Honor constant default
  member initializers and zero-initialize omitted fields.
- [x] Substitute nested parameter-object member reads such as `Value.leaf.first`
  through arbitrary nested paths; support a sole null ordinary-pointer leaf
  inside nested structural subobjects and match GCC's omitted null-literal
  wrapper encoding.
- [x] Compare exact GCC/RCC Itanium symbols on i686 and AMD64 and execute both
  generated AMD64 and GCC-host programs. The focused structural-owner target
  and full native-Windows MinGW `test-cxx` and `test-debug-info` aggregates
  pass with exit 0.
- [x] Commit implementation and regressions as `b59660d` on local RCC `main`;
  no push.
- [ ] Non-null pointer-valued members and remaining standard-valid explicit
  member-specialization forms remain open.

## RCC continuation (2026-10-16, multi-level nested explicit specialization)

- [x] Resolve explicit nested member-template specializations through an
  intermediate specialized member-class-template owner, such as
  `Owner<int>::Mid<char>::Inner<long>`, and resolve the same multi-level type-id
  when naming primary or unspecialized member specializations.
- [x] Compare all three GCC Itanium symbols against RCC on i686/AMD64 and run
  both the generated AMD64 and GCC-host programs. The focused nested-owner gate
  and full native-Windows MinGW `test-cxx` aggregate pass (exit 0).

## RCC continuation (2026-10-16, three-level nested explicit specialization)

- [x] Accept three consecutive explicit-specialization template heads for a
  nested member specialization with two intermediate class-template owners,
  such as `Owner<int>::First<char>::Mid<short>::Inner<long>`. Resolve the
  intermediate owners in both specialization declarations and ordinary nested
  type-ids.
- [x] Compare all three GCC Itanium symbols against RCC on i686/AMD64 and run
  generated AMD64 plus GCC-host programs. The focused nested-owner gate and
  full native-Windows MinGW `test-cxx` aggregate pass (exit 0).

## RCC continuation (2026-10-16, four-level nested explicit specialization)

- [x] Resolve explicit member specialization through three intermediate
  nested class-template owners and consume the corresponding four consecutive
  explicit-specialization template heads, using
  `Owner<int>::First<char>::Second<short>::Third<long>::Inner<double>` as the
  regression case.
- [x] Compare all three GCC Itanium symbols against RCC on i686/AMD64 and run
  generated AMD64 plus GCC-host programs. The focused nested-owner gate passes;
  the full native-Windows MinGW `test-cxx` aggregate passes with the same parser
  implementation (exit 0).
- [x] Verify a standard-valid explicit member specialization across five
  nested member-template levels (see the five-level explicit-specialization
  continuation below).
- [ ] Non-null structural pointer NTTP values remain open.

## RCC continuation (2026-10-16, partial specialization through nested partial owners)

- [x] Parse consecutive template heads for `Owner<T*>::Mid<U*>::Leaf<V*>`
  partial specializations and retain each owner-pattern parameter context.
  Resolve the dependent `Mid<U*>` owner to its matching nested partial-class
  definition before looking up and specializing `Leaf<V*>`.
- [x] Cover primary, outer-partial, intermediate-partial, and leaf-partial
  selections. Compare all four GCC Itanium symbols on i686/AMD64 and execute
  generated AMD64 plus GCC-host programs; the focused nested-owner gate passes
  (exit 0).
- [ ] Additional dependent owner forms and non-null structural pointer NTTP
  values remain open.

## RCC continuation (2026-10-16, deeper nested partial owner chain)

- [x] Extend nested partial owner matching through
  `Owner<T*>::Mid<U*>::Sub<V*>::Leaf<W*>`, covering primary, outer-,
  intermediate-, deeper-intermediate-, and leaf-partial selections.
- [x] Compare all five exact GCC Itanium symbols against RCC on i686/AMD64 and
  execute the generated AMD64 and GCC-host programs. The focused nested-owner
  target passes (exit 0).
- [ ] Additional dependent owner forms and non-null structural pointer NTTP
  values remain open.

## RCC continuation (2026-10-16, explicit member specialization below a specialized intermediate owner)

- [x] Specialize `Inner<long>` after explicitly specializing its enclosing
  member class template `Mid<char>` inside `Owner<int>`. Retain the primary
  `Owner<long>::Mid<char>` case and the unspecialized inner-template case for
  `Owner<int>::Mid<char>`.
- [x] Compare the three exact GCC Itanium symbols against RCC on i686/AMD64 and
  execute both the generated AMD64 program and GCC-host program. The focused
  nested-owner target passes (exit 0).
- [x] Explicitly specialize a nested `Leaf<long>` through an outer partial
  specialization (`Owner<int*>::Mid<char>::Leaf<long>`), while checking the
  primary outer owner and unspecialized `Mid`/`Leaf` selections. Compare all
  four exact GCC Itanium symbols on i686/AMD64 and execute both host programs;
  the focused nested-owner target passes (exit 0).
- [x] Verify a five-level explicit member-specialization chain that passes
  through an explicitly specialized intermediate owner (see the continuation
  below).
- [ ] Non-null structural pointer NTTP values remain open.

## RCC continuation (2026-10-16, five-level nested explicit specialization)

- [x] Resolve an explicit nested member specialization through four nested
  member-class-template owners, consuming five consecutive explicit
  specialization template heads for `Owner<int>::First<char>::Second<short>`
  `::Third<int>::Fourth<float>::Inner<double>`.
- [x] Retain the primary and explicitly specialized outer-owner cases; compare
  all three exact GCC Itanium symbols against RCC on i686/AMD64 and execute the
  generated AMD64 and GCC-host programs. The focused nested-owner target passes
  (exit 0).
- [ ] Explicit-specialization chains deeper than seven nested levels and
  non-null structural pointer NTTP values remain open.

## RCC continuation (2026-10-16, deep specialization below a specialized intermediate owner)

- [x] Explicitly specialize `First<char>` below `Owner<int>`, then explicitly
  specialize `Inner<double>` through `Second<short>::Third<int>::Fourth<float>`.
  Retain primary-owner, explicit outer-owner, and unspecialized nested-member
  cases.
- [x] Compare all four exact GCC Itanium symbols against RCC on i686/AMD64 and
  execute generated AMD64 and GCC-host programs. The focused nested-owner
  target passes (exit 0).
- [ ] Explicit-specialization chains deeper than seven nested levels and
  non-null structural pointer NTTP values remain open.

## RCC continuation (2026-10-16, five nested partial-owner levels)

- [x] Match the primary and six progressively nested partial specializations
  through `Owner<T*>::First<U*>::Second<V*>::Third<W*>::Fourth<X*>::Leaf<Y*>`,
  covering five nested member-template levels.
- [x] Compare all seven exact GCC Itanium symbols against RCC on i686/AMD64 and
  execute the generated AMD64 and GCC-host programs. The focused nested-owner
  target passes (exit 0).
- [ ] Additional dependent owner forms, explicit-specialization chains deeper
  than six nested levels, and non-null structural pointer NTTP values remain
  open.

## RCC continuation (2026-10-16, mixed type/value nested partial owners)

- [x] Match owner, `Mid`, and `Leaf` partial specializations constrained by
  both dependent pointer types and integral values (`Owner<T*, 7>`,
  `Mid<U*, 11>`, and `Leaf<V*, 13>`), retaining primary and each outer-to-leaf
  selection.
- [x] Compare all four exact GCC Itanium symbols against RCC on i686/AMD64 and
  execute generated AMD64 and GCC-host programs. The focused nested-owner
  target passes (exit 0).
- [ ] Other dependent owner patterns (including expression, array/reference,
  and template-template forms) and non-null structural pointer NTTP values
  remain open.

## RCC continuation (2026-10-16, nested owner partial ordering)

- [x] Verify nested owner selection with the more-specialized `const T*` outer
  specialization declared before the less-specialized `T*` form, including
  each owner's matching nested `Mid<U*>` specialization.
- [x] Compare all five GCC Itanium symbols against RCC on i686/AMD64 and run
  generated AMD64 and GCC-host programs. The focused nested-owner target passes
  (exit 0).
- [ ] Other dependent owner patterns (including expression, array/reference,
  and template-template forms) and non-null structural pointer NTTP values
  remain open.

## RCC continuation (2026-10-16, array type-id nested partial owners)

- [x] Parse abstract declarators in class partial-specialization arguments,
  nested member-specialization arguments, and ordinary class-template-ids so
  constant-bound array types survive as complete type arguments.
- [x] Cover array patterns at the outer owner, nested `Mid`, and `Leaf` levels;
  match all four exact GCC Itanium symbols on i686/AMD64 and execute generated
  AMD64 plus GCC-host programs. The focused nested-owner target passes
  (exit 0).
- [x] Correct array type mangling to include the Itanium ABI separator after
  the dimension; RCC's names now match GCC for all four tested instantiations.
- [ ] Additional dependent array-bound expressions beyond the verified `+`,
  `*`, `<<`, signed `>>`, conditional-comparison, and logical short-circuit
  forms;
  reference/array-reference and
  template-template owner forms, non-null structural pointer NTTP values, and
  explicit owner-specialization chains deeper than seven levels remain open.

## RCC continuation (2026-10-16, reference-to-array nested partial owners)

- [x] Preserve lvalue references to constant-bound arrays as complete type-id
  arguments in outer class-template partial specializations and nested `Mid` /
  `Leaf` partial specializations.
- [x] Verify primary, outer, intermediate, and leaf selection; compare all four
  exact GCC Itanium symbols with RCC on i686/AMD64 and execute generated AMD64
  and GCC-host programs. The full focused nested-owner gate passes (exit 0).
- [ ] Other reference type-id combinations, additional dependent array-bound
  expressions beyond the verified arithmetic (including signed `>>`),
  conditional, and logical short-circuit forms,
  template-template owners, non-null structural pointer NTTP values, and
  explicit owner-specialization chains deeper than seven levels remain open.

## RCC continuation (2026-10-16, rvalue-reference-to-array nested partial owners)

- [x] Parse rvalue references to constant-bound arrays in nested owner partial
  specializations, including the outer owner, `Mid`, and `Leaf` patterns.
- [x] Verify primary, outer, intermediate, and leaf selection; compare all four
  exact GCC Itanium symbols with RCC on i686/AMD64 and execute generated AMD64
  and GCC-host programs. The focused target and aggregate `test-cxx`
  / `test-debug-info` gates pass (exit 0).
- [ ] Other reference type-id combinations, additional dependent array-bound
  expressions beyond the verified arithmetic (including signed `>>`),
  conditional, and logical short-circuit forms,
  template-template owners, non-null structural pointer NTTP values, and
  explicit owner-specialization chains deeper than seven levels remain open.

## RCC continuation (2026-10-16, six-level nested explicit specialization)

- [x] Resolve an explicit member specialization through five intermediate
  member-class-template owners, consuming six consecutive explicit
  specialization heads for `Owner<int>::First<char>::Second<short>::Third<int>`
  `::Fourth<float>::Fifth<long>::Inner<double>`.
- [x] Retain the primary and explicit outer-owner cases; compare all three exact
  GCC Itanium symbols with RCC on i686/AMD64 and execute the generated AMD64 and
  GCC-host programs. The focused nested-owner target passes (exit 0).
- [ ] Explicit-specialization chains deeper than seven nested levels and
  non-null structural pointer NTTP values remain open.

## RCC continuation (2026-10-16, dependent array-bound nested partial owners)

- [x] Match dependent `T[N + 1]`, `T[2 * N + 1]`, and `T[(N << 1) + 1]`
  array-bound expressions at outer-owner, nested `Mid`, and `Leaf`
  partial-specialization levels. Defer expression validation until later
  non-type arguments have been deduced, and deduce a direct `T[N]` extent from
  the actual fixed array type.
- [x] Verify primary and outer/intermediate/leaf selection for matching and
  mismatching extents; compare all six exact GCC Itanium symbols against RCC
  on i686/AMD64 and execute generated AMD64 plus GCC-host programs. The focused
  nested-owner gate and aggregate `test-cxx test-debug-info` both pass (exit 0).
- [x] Evaluate `T[(N == 0) ? 1 : (N * 2)]` at an outer owner and its nested
  `Inner`, covering both conditional branches, primary fallback, and nested
  partial selection. Match all four exact GCC Itanium symbols on i686/AMD64 and
  execute generated AMD64 plus GCC-host programs; the focused nested-owner
  gate passes (exit 0).
- [ ] Additional dependent array-bound operators/forms beyond the verified
  addition, multiplication, left-shift, signed right-shift, conditional-
  comparison, and logical short-circuit cases,
  other reference type-id and template-template owner forms, non-null
  structural pointer NTTP values, and explicit owner-specialization chains
  deeper than seven levels remain open.

## RCC continuation (2026-10-16, signed right-shift in dependent array bounds)

- [x] Evaluate negative signed right-shift operands arithmetically as required
  by C++20 without relying on the host's implementation-defined signed shift
  behavior or overflowing when the left operand is `INT64_MIN`.
- [x] Cover `-1`, `-3`, and positive right-shift operands at dependent array
  bounds, including nested member partial selection and primary fallback;
  test `LLONG_MIN` shifted by 63 without signed overflow; compare all seven
  exact GCC Itanium symbols against RCC on i686/AMD64 and run the focused
  generated AMD64 and GCC-host programs.
- [ ] Additional dependent array-bound operators beyond the verified
  addition, multiplication, left-shift, signed right-shift, conditional-
  comparison, and logical short-circuit cases, other reference type-id and
  template-template owner forms, non-null structural pointer NTTP values, and
  explicit owner-specialization chains deeper than seven levels remain open.

## RCC continuation (2026-10-16, seven-level nested explicit specialization)

- [x] Resolve an explicit member specialization through six intermediate
  member-class-template owners, consuming seven consecutive explicit
  specialization template heads for `Owner<int>::First<char>::Second<short>`
  `::Third<int>::Fourth<float>::Fifth<long>::Sixth<char>::Inner<double>`.
- [x] Retain the primary and explicitly specialized outer-owner cases; compare
  all three exact GCC Itanium symbols against RCC on i686/AMD64 and execute the
  generated AMD64 and GCC-host programs. The focused nested-owner target passes
  (exit 0).
- [x] Extend explicit nested member specialization beyond seven levels with
  eight consecutive explicit specialization template heads; retain primary and
  explicitly specialized outer-owner cases, compare all three exact GCC
  Itanium symbols on i686/AMD64, and run generated AMD64 plus GCC-host programs.
- [ ] Explicit-specialization chains deeper than twelve nested levels and
  non-null structural pointer NTTP values remain open.

## RCC continuation (2026-10-16, eight-level nested explicit specialization)

- [x] Add primary-owner, explicit outer-owner, and explicit deepest-member cases
  through `Owner<int>::First<char>::Second<short>::Third<int>::Fourth<float>`
  `::Fifth<long>::Sixth<char>::Seventh<float>::Inner<double>`.
- [x] Verify all three exact GCC Itanium symbols against RCC for i686/AMD64 and
  run the focused generated AMD64 and GCC-host programs.
- [ ] Chains deeper than twelve levels and non-null structural pointer NTTP
  values remain open.

## RCC continuation (2026-10-16, logical short-circuit in dependent array bounds)

- [x] Evaluate `&&` and `||` in dependent array-bound expressions with C++
  short-circuit semantics, skipping an unsupported non-constant call in an
  unselected operand while evaluating supported selected paths.
- [x] Cover zero/nonzero extents, matching outer and nested `Inner<Value*>`
  partial specializations, and primary fallback; compare all eight exact GCC
  Itanium symbols against RCC on i686/AMD64 and execute generated AMD64 plus
  GCC-host programs. The focused nested-owner target passes (exit 0).
- [ ] Other dependent array-bound expression operators, type-conversion and
  overflow-diagnostic cases, non-null structural pointer NTTP values, and
  explicit-specialization chains deeper than twelve nested levels remain open.

## RCC continuation (2026-10-16, checked arithmetic in dependent array bounds)

- [x] Evaluate dependent subtraction, signed division and remainder, bitwise
  AND/OR/XOR/complement, and unary negation when matching nested owner
  partial-specialization array bounds. Cover positive and negative division
  with C++ truncation/remainder signs, primary fallback, and nested member
  selection.
- [x] Guard signed addition, subtraction, multiplication, negation, division,
  and remainder evaluation against host-language overflow/undefined behavior;
  return “not evaluable” for unrepresentable intermediate results instead of
  selecting a specialization from a wrapped value.
- [x] Compare the added GCC Itanium symbols for both i686 and AMD64 and run
  generated AMD64 plus GCC-host programs. The focused nested-owner target and
  aggregate `test-cxx test-debug-info` both pass (exit 0).
- [ ] Cover unsigned conversions outside the evaluator's signed 64-bit range
  plus floating casts; non-null structural pointer NTTP values and
  explicit-specialization chains deeper than twelve nested levels remain open.

## RCC continuation (2026-10-16, narrowing casts in dependent array bounds)

- [x] Apply the target-width conversion for dependent integral
  `static_cast<unsigned char>(N)` before evaluating an array-bound expression;
  test `N == 256` wrapping to zero and the resulting `int[1]` partial match.
- [x] Verify mismatched primary fallback and nested `Inner<Value*>` selection,
  compare exact GCC Itanium symbols on i686/AMD64, and run generated AMD64 plus
  GCC-host programs. The focused nested-owner target and aggregate
  `test-cxx test-debug-info` both pass (exit 0).
- [x] Verify signed narrowing (`255` to `signed char`), both boolean results,
  and a fixed-underlying enum cast through its `unsigned char` representation;
  compare exact GCC Itanium symbols on i686/AMD64 and run generated AMD64 plus
  GCC-host programs. The focused nested-owner target passes (exit 0).
- [ ] Cover unsigned conversions outside the evaluator's signed 64-bit range
  and floating casts; non-null structural pointer NTTP values and
  specialization chains deeper than twelve nested levels remain open.

## RCC continuation (2026-10-16, nine-level nested explicit specialization)

- [x] Add primary-owner, explicit outer-owner, and explicit deepest-member
  cases through `Owner<int>::First<char>::Second<short>::Third<int>`
  `::Fourth<float>::Fifth<long>::Sixth<char>::Seventh<float>::Eighth<long>`
  `::Inner<double>`.
- [x] Compare all three exact GCC Itanium symbols against RCC for i686 and
  AMD64, and run the generated AMD64 and GCC-host programs. The focused
  nested-owner target passes (exit 0).
- [ ] Chains deeper than twelve levels and non-null structural pointer NTTP
  values remain open.

## RCC continuation (2026-10-16, dependent array-bound overflow diagnostics)

- [x] Distinguish signed overflow from an unevaluable dependent expression in
  class-template member array bounds. Diagnose overflowing addition,
  subtraction, multiplication, unary negation, division, and remainder rather
  than silently leaving an invalid dependent bound unresolved.
- [x] Add six GCC-invalid regressions covering positive/negative overflow and
  `INT64_MIN / -1` plus `% -1`; verify six diagnostics for both i686 and AMD64,
  no `.ro` artifact, and GCC rejection. The focused overflow target passes.
- [ ] Unsigned conversions outside the evaluator's signed 64-bit range,
  floating casts, non-null structural pointer NTTP values, specialization
  chains deeper than twelve levels, and overflow contexts outside this array-
  bound substitution path remain open.

## RCC continuation (2026-10-16, unsigned-64 dependent array bounds)

- [x] Detect a dependent array bound whose final cast converts signed `-1` to
  `unsigned long long`; diagnose the resulting `UINT64_MAX` bound as out of
  range instead of accepting an unresolved type.
- [x] Compare the invalid case against GCC, check the diagnostic and absence
  of `.ro` output for both i686 and AMD64. The focused unsigned-64 bound target
  passes.
- [x] Preserve the target-width unsigned bit pattern while evaluating dependent
      member array bounds through nested casts and larger integer expressions.
      Verify unsigned addition/subtraction, modulo wrap to a zero bound, unary
      negation, unsigned comparison, and later narrowing to `unsigned char`.
      Both valid and invalid forms pass the GCC comparison and the focused
      i686/AMD64 regressions.
- [ ] Other direct unsigned non-type template argument types and conversion
      forms above `INT64_MAX`, floating casts, non-null structural pointer
      NTTP values, and overflow contexts outside dependent array-bound
      substitution remain open.

## RCC continuation (2026-10-16, direct unsigned-64 template arguments)

- [x] Instantiate a dependent member array from direct `ULLONG_MAX` non-type
  template arguments. Verify equality and unsigned wraparound select the GCC-
  matching extents on i686 and AMD64; GCC C++20 accepts the same fixture.
- [x] Reject direct `ULLONG_MAX` as an array extent on both targets, require
  exactly four out-of-range diagnostics including this case, verify no `.ro`
  artifact, and compare against GCC rejection. Both focused tests are in the
  `test-cxx`/`test-ci` regression path.
- [ ] Other direct unsigned argument types/conversion forms above
      `INT64_MAX`, floating casts, and unsigned constant-expression contexts
      outside dependent array-bound substitution remain open.

## RCC continuation (2026-10-16, ten-level nested explicit specialization)

- [x] Add primary-owner, explicitly specialized outer-owner, and explicitly
  specialized deepest-member cases through `First<char>::Second<short>`
  `::Third<int>::Fourth<float>::Fifth<long>::Sixth<char>::Seventh<float>`
  `::Eighth<long>::Ninth<short>::Inner<double>`.
- [x] Compare all three exact GCC Itanium symbols against RCC for i686 and
  AMD64; compile both target objects, execute the generated AMD64 entry and the
  GCC-host program. `test-cxx-nested-class-template-owner-explicit-chain-10`
  passes (exit 0) and is a dependency of the aggregate nested-class suite.
- [ ] Chains deeper than twelve levels and non-null structural pointer NTTP
  values remain open.

## RCC continuation (2026-10-16, unsigned-64 template expression operations)

- [x] Carry the target-width unsigned value through direct `ULLONG_MAX`
  template arguments in dependent array bounds. Verify unsigned comparison,
  multiplication, division, remainder, bitwise AND/OR/XOR/complement, shifts,
  and narrowing to `unsigned char`; GCC accepts the matching C++20 fixture.
- [x] Run `make test-cxx test-debug-info` with the expanded direct-argument
  fixture and evaluator changes. The complete C++ and debug-info aggregates
  pass (exit 0).
- [ ] Other unsigned argument types/conversion paths and constant-expression
      contexts outside dependent array bounds remain open; non-null structural
      pointer NTTP values and specialization chains deeper than twelve levels also
      remain open.

## RCC continuation (2026-10-16, floating conversions in dependent array bounds)

- [x] Evaluate dependent floating arithmetic, conditional expressions, and
  floating casts while substituting class-template member array bounds. Cover
  `double`/`float` rounding, signed truncation toward zero, unsigned conversion,
  and boolean conversion for both i686 and AMD64.
- [x] Reject out-of-range floating-to-integer constant conversions as invalid
  dependent array bounds with the overflow diagnostic; confirm no `.ro` output
  and compare the positive and negative cases with GCC C++20.
- [x] Run `make test-cxx test-debug-info`; both aggregate targets pass (exit 0).
- [ ] Extend floating constant evaluation and conversion coverage to other
  constant-expression contexts; structural pointer NTTP values and
  specialization chains deeper than twelve levels remain open.

## RCC continuation (2026-10-16, dependent signed shift validation)

- [x] Evaluate dependent signed shifts using the promoted left operand width,
      while independently promoting and preserving the right operand width.
- [x] Reject invalid counts, negative signed left operands, and signed
      left-shift results that are not representable in the corresponding
      unsigned type; compare invalid cases with GCC C++20 on both targets.
- [x] Run the complete `test-cxx` and `test-debug-info` aggregate targets; both
      pass (exit 0).
- [ ] Other constant-expression contexts, non-null structural pointer NTTP
      values, and specialization chains deeper than twelve levels remain open.

## RCC continuation (2026-10-16, Linux CI nested-class coverage)

- [x] Add a POSIX CI gate for nested class-template coverage, including the five
      specialization fixtures not reachable from Linux `test-ci`: compile
      i686 and AMD64 outputs, execute each RCC AMD64 program and GCC host
      counterpart, and compare the complete GCC/RCC Itanium function-symbol
      sets for both target architectures.
- [x] Gate nested-class access and missing-owner negative cases on both target
      architectures, requiring the expected diagnostics, no rejected `.ro`
      output, and matching GCC rejection.
- [x] Add the `#line` regression to the Linux production CI workflow. Validate
      the nested fixtures and target outputs in WSL with GCC; the workflow runs
      this gate in both GCC and Clang regression jobs.

## RCC continuation (2026-10-16, twelve-level nested explicit specialization)

- [x] Add primary-owner, explicit outer-owner, and explicit deepest-member
      cases through `First<char>::Second<short>::Third<int>::Fourth<float>`
      `::Fifth<long>::Sixth<char>::Seventh<float>::Eighth<long>::Ninth<short>`
      `::Tenth<int>::Eleventh<unsigned char>::Inner<double>`.
- [x] Compare the complete GCC/RCC Itanium function-symbol sets for i686 and
      AMD64, assemble both target outputs, and execute the generated AMD64 and
      GCC-host programs in the POSIX CI regression script.
- [ ] Chains deeper than twelve levels and non-null structural pointer NTTP
      values remain open.

## RCC continuation (2026-10-16, structural pointer NTTP values)

- [x] Preserve non-null object and function addresses in structural class
  non-type template arguments using translation-unit-local declaration tokens;
  restore them as relocatable address expressions during nested template
  cloning instead of treating host addresses as target values.
- [x] Emit GCC-compatible Itanium encodings for external and internal-linkage
  object targets, preserve `nullptr` and integer-zero null pointer values, and
  distinguish separate object/function-pointer specializations.
- [x] Add the regression to the POSIX nested-class CI gate. Compare every
  Itanium-prefixed fixture symbol with GCC on i686/AMD64, assemble both RCC
  outputs, and execute RCC AMD64 plus the GCC-host program; the complete
  `scripts/ci_nested_class_templates.sh` gate passes in WSL.
- [ ] Additional pointer constant-expression forms, function-pointer
      conversion cases, and reference members remain open.

## RCC continuation (2026-10-16, computed structural pointer NTTP expressions)

- [x] Preserve pointer values selected by integral constant conditional
      expressions, including a selected null pointer; accept identity-preserving
      `&object + 0`, `0 + &object`, and `&object - 0` forms and same-type
      `static_cast` expressions.
- [x] Add nested structural object- and function-pointer regressions. Compare
      the complete GCC/RCC Itanium symbol sets on i686 and AMD64, assemble both
      outputs, execute RCC AMD64 and GCC-host programs, and pass the full POSIX
      nested-class CI gate.
- [x] Select an exact-match non-template overload from the structural member's
      function-pointer type; verify GCC-compatible symbols and runtime result.
- [x] Resolve overloaded function-template addresses from the structural
      member's target function-pointer type, rank viable ordinary candidates,
      and preserve the non-template tie preference. GCC/RCC symbol comparisons,
      i686/AMD64 generation, and RCC/GCC host execution pass.
- [x] Add the fixture to the native-Windows nested-template gate; the focused
      gate and full `test-cxx` aggregate pass, and the complete POSIX
      `scripts/ci_nested_class_templates.sh` gate passes.
- [x] Correct the C++ global-variable linkage expectation and use the portable
      expected-failure macro in debug-info tests; `test-cxx-language-linkage`
      and `test-debug-info` pass.
- [x] Keep `rinvalidate` host objects separate under `obj/windows` and
      `obj/posix`; build both variants and validate current RCC-generated RIN,
      RLL, and DRV artifacts.
- [x] Re-run the complete native-Windows MinGW `test-cxx` aggregate against the
      current compiler tree; `test-cxx-member-lifetime` and the full aggregate
      both pass (exit 0).
- [ ] Additional pointer constant-expression forms and function-pointer
      conversion cases remain open.

## RCC continuation (2026-10-16, structural data-member-pointer NTTP values)

- [x] Preserve null and non-null data-member-pointer values inside a structural
  class NTTP with translation-unit-local owner/member/offset tokens; restore
  member designators in nested template instantiations without host pointer
  addresses.
- [x] Match GCC Itanium encodings for data-member addresses and null values,
  including the class-type substitution contributed by a nested structural
  NTTP owner. Compare complete symbols on i686/AMD64, assemble both targets,
  and execute RCC AMD64 and GCC-host programs.
- [x] Add the regression to `scripts/ci_nested_class_templates.sh`; the complete
  POSIX nested-class CI gate passes in WSL.
- [ ] Additional standard-valid structural class NTTP forms remain open.

## RCC continuation (2026-10-16, structural pointer subobject offsets)

- [x] Preserve byte offsets, array-subobject identity, and one-past state for
  structural class pointer NTTPs; keep distinct locations distinct in
  specialization and typeinfo identity, and reconstruct target-relocatable
  expressions when cloning nested template arguments.
- [x] Support named static/global one-dimensional array elements and array
  one-past pointers formed by indexing, pointer arithmetic, and equivalent
  address spellings; support one-past pointers to a named scalar object.
- [x] Check expected Itanium encodings and add a regression for i686 and AMD64
  symbols, assembly of both target outputs, and AMD64 execution. Verify the
  focused Windows and WSL POSIX gates plus related structural NTTP regressions.
- [x] Commit implementation, fixture, and Makefile gate as `1ad666d` on RCC
  `main`; do not push or stage the parent repository's gitlink.
- [x] Extend the location model to nested static/global array index paths and
  constrain pointer arithmetic to the immediately selected inner array,
  including its one-past position. Support decay of a selected static/global
  row to an element pointer while retaining that row's bounds and subobject
  identity through pointer arithmetic.
- [x] Check row-decay start and one-past values, distinct Itanium encodings,
  i686/AMD64 output, AMD64 execution, and rejection of both direct and decayed
  pointer arithmetic beyond the selected row in Windows and WSL CI.
- [x] Re-run native-Windows `mingw32-make SHELL=cmd.exe test-cxx` after the
  array-decay parser change; the full aggregate passes.
- [x] Parse pointer-to-array declarators on C++ class fields and preserve
  structural NTTP pointers to nested static/global row arrays, including the
  one-past position of the containing matrix. Verify typed array-path Itanium
  encodings, i686/AMD64 generation, AMD64 execution, and out-of-matrix
  rejection in the Windows fixture and WSL POSIX gate.
- [x] Re-run the complete native-Windows MinGW `test-cxx` aggregate after the
  pointer-to-array class-field parser change; it passes (exit 0).
- [x] Support recursively selected named global aggregate members and array
  elements with typed member/index paths, including member arrays and arrays
  of aggregates; use Itanium `dt`/`ix` expression encodings rather than the
  previous byte-offset extension. Normalize pointer arithmetic that crosses
  a selected row to the enclosing array element when it matches GCC's
  accepted structural-pointer value.
- [x] Differentially compare every GCC-emitted structural-pointer symbol on
  i686 and AMD64 for nested-array and aggregate-member fixtures; assemble both
  RCC targets and execute the RCC and GCC AMD64 programs in Windows and WSL.
  The subset check accounts for RCC currently emitting unused member functions
  of an instantiated class template.
- [ ] Extend compiler-to-compiler comparisons to computed pointer expressions
  GCC rejects (including some flat-array and aggregate-member pointer
  arithmetic); Itanium ABI issue #47 remains open.

## RCC continuation (2026-10-16, selected null pointer conditions and 13-level chains)

- [x] Recursively evaluate the selected arm of a structural pointer NTTP
  conditional, including selected `nullptr` and integer-zero null values. The
  existing computed-pointer regression passes in the Windows native build and
  the WSL POSIX nested-class CI gate.
- [x] Add a 13-level nested explicit-specialization regression covering the
  primary owner, an explicitly specialized owner, and an explicitly
  specialized deepest member. Compare GCC/RCC symbols on i686 and AMD64,
  assemble both outputs, and execute both generated AMD64 programs in the POSIX
  gate; the Windows build also compiles both target outputs and runs AMD64.
- [x] Re-run the complete native-Windows MinGW `test-cxx` aggregate after the
  parser change; it passes (exit 0).
- [ ] Deeper specialization chains and the other structural pointer forms
  listed above remain open.

## RCC continuation (2026-10-16, structural member-function-pointer NTTP values)

- [x] Preserve a selected member-function identity in structural class NTTP
  values and nested template instantiations; clone it as a method designator
  rather than a null integer value.
- [x] Emit GCC-compatible Itanium encodings for structural member-function
  pointer values, including the member-function type, method qualifiers, and
  nested-name substitutions. Verify complete symbols against GCC on i686 and
  AMD64, assemble both targets, and execute the AMD64 regression.
- [x] Parse parenthesized pointer-to-member function declarators as class
  fields and add the regression to `scripts/ci_nested_class_templates.sh`.
  The complete POSIX nested-class CI gate passes in WSL.
- [x] Re-run native-Windows MinGW `test-cxx` after the parser and structural
  NTTP changes; the full aggregate passes (exit 0).
- [ ] Other standard-valid structural class NTTP forms remain open.

## RCC continuation (2026-10-16, multiple structural member-pointer NTTP leaves)

- [x] Preserve multiple nested data-member-pointer leaves and multiple
  member-function-pointer leaves in structural class NTTP values, including
  owner/member identity through template cloning.
- [x] Emit GCC-compatible Itanium names for mixed/nested leaves, including
  repeated owner and member-function type substitutions. Compare fixture
  symbol sets on i686/AMD64, assemble both targets, and execute the AMD64
  RCC/GCC-host regressions.
- [x] Add both regressions to `scripts/ci_nested_class_templates.sh`; the WSL
  POSIX gate passes.
- [x] Re-run the complete native-Windows MinGW `test-cxx` aggregate; it passes
  (exit 0).
- [ ] Other standard-valid structural class NTTP forms remain open.

## RCC continuation (2026-10-16, structural class NTTP reference members)

- [x] Preserve reference members bound to namespace-scope constant objects and
      named aggregate subobjects and array elements inside nested structural
      class NTTP values. Restore the referent as an lvalue when cloning
      template member access, while keeping reference identity distinct from
      pointer values.
- [x] Add direct-object and aggregate-subobject references to
      `tests/cxx_structural_reference_members_nttp.cpp`; compare complete
      GCC/RCC Itanium symbols on i686/AMD64, assemble both targets, and execute
      RCC/GCC AMD64 programs. Array-element reference paths pass the same
      differential and runtime checks.
- [x] Add the regression to `scripts/ci_nested_class_templates.sh`; the full
      WSL POSIX gate passes.
- [x] Re-run the complete native-Windows MinGW `test-cxx` aggregate after the
      parser and template-cloning changes; it passes (exit 0).
- [ ] Other standard-valid structural class NTTP forms remain open.

## RCC continuation (2026-10-16, structural NTTP function references)

- [x] Accept a structural class NTTP reference member bound directly to a
      namespace-scope function, preserving the function designator and
      invocation through nested template member access.
- [x] Correct global-scope Itanium name mangling when the parser supplies the
      root namespace node; retain nested names for named namespaces and class
      scopes. Compare GCC/RCC symbols on i686 and AMD64 and assemble both.
- [x] Add `tests/cxx_structural_function_reference_nttp.cpp` to
      `scripts/ci_nested_class_templates.sh`; the full WSL POSIX gate passes.
- [x] Re-run the complete native-Windows MinGW `test-cxx` aggregate; it passes
      (exit 0).
- [ ] Other standard-valid structural class NTTP forms remain open.

## RCC continuation (2026-10-16, structural NTTP array references)

- [x] Accept a structural class NTTP reference member bound to a complete
      namespace-scope array, preserve its identity through nested template
      access, and index the referenced array in instantiated member functions.
- [x] Add `tests/cxx_structural_array_reference_nttp.cpp` to
      `scripts/ci_nested_class_templates.sh`; compare GCC/RCC symbols, assemble
      i686 and AMD64 outputs, and execute RCC/GCC AMD64 programs in the full
      WSL POSIX gate.
- [x] Assemble the generated i686 and AMD64 outputs with native MinGW and run
      both RCC- and GCC-generated AMD64 programs on Windows.
- [ ] Other standard-valid structural class NTTP forms remain open.

## RCC continuation (2026-10-16, pointer-bearing structural NTTP packs)

- [x] Cover a structural class NTTP pack whose values contain non-null object
      pointers and integral fields, including a nested class-template partial
      specialization. Compare emitted Itanium symbols and execute GCC/RCC
      outputs for i686 and AMD64 through `scripts/ci_nested_class_templates.sh`.
- [x] Support a parenthesized expression pattern in a binary right fold over
      a structural class NTTP pack; verify member access, per-element
      substitution, GCC/RCC symbol parity, i686 assembly, and AMD64 execution
      in the WSL POSIX gate.
- [x] Run the complete native-Windows MinGW `test-cxx` aggregate after the
      parser change; it passes (exit 0).
- [ ] Other structural NTTP pack member shapes and additional multi-pack
      expression expansions remain open.

## RCC continuation (2026-10-16, structural NTTP bit-fields)

- [x] Pack and unpack named signed, unsigned, and enum bit-field values inside
      structural class NTTPs; preserve sign extension and match GCC's
      bit-width-specific Itanium value types.
- [x] Compare complete GCC/RCC symbol sets, assemble i686 and AMD64 output,
      and execute both AMD64 programs through
      `scripts/ci_nested_class_templates.sh`; the full WSL POSIX gate passes.
- [x] Cover C++ unnamed zero-width bit-fields as allocation-unit alignment
      barriers. GCC's C++20 `-Werror` execution and `test-cxx-bitfields` pass,
      including i686/AMD64 generation and the RCC-generated x64 host runner.
- [x] Extend C and C++ bit-field allocation units and read/modify/write code
      generation through eight-byte base types on i686 and AMD64. Cover fields
      crossing bit 32, signed extension, 40-bit and full 64-bit widths, static
      and local aggregate initialization, compound arithmetic, shifts, and
      post-increment. GCC strict C17/C++20 oracles, `test-bitfields`,
      `test-cxx-bitfields`, and the full native-Windows `test-cxx` aggregate
      pass; both target runtime paths are exercised.

## RCC continuation (2026-10-16, structural NTTP nullptr members)

- [x] Accept a `decltype(nullptr)` member in a structural class NTTP, preserve
      the null value through packing and member access, and emit the GCC
      Itanium representation.
- [x] Compare GCC/RCC symbols, assemble i686 and AMD64 output, and execute
      both AMD64 programs through the complete WSL nested-class gate.
- [x] Rebuild the MinGW compiler, assemble its i686 output, and execute both
      RCC-generated and GCC-generated AMD64 programs on native Windows.
- [ ] Other standard-valid structural class member types remain open.

## RCC continuation (2026-10-16, structural NTTP floating members)

- [x] Preserve public `float` and `double` data-member values in structural
      class NTTPs, including template member access and GCC-compatible
      floating-value Itanium mangling.
- [x] Enforce exact-value rules for integer/floating conversions in aggregate
      initialization; reject non-exact narrowing and retain exact conversions.
- [x] Compare GCC/RCC symbols, assemble i686 and AMD64 output, and execute
      both AMD64 programs through the complete WSL nested-class gate.
- [x] Rebuild the MinGW compiler, assemble its i686 output, and execute both
      RCC-generated and GCC-generated AMD64 programs natively on Windows.
- [ ] Non-finite values, extended precision/rounding cases beyond the current
      RinOS binary64 `long double` ABI, and broader floating constant-expression
      conversions remain open.
  - [x] Execute long-double call/return, variadic retrieval, and binary64
        tie-to-even constant rounding with the legacy backend for C17 and C++20
        on i686 and AMD64. Verified-backend C17/C++20 objects compile for both
        targets; verified runtime execution remains open.

## RCC continuation (2026-10-16, non-final class-template-id pack expansions)

- [x] Parse and substitute type, non-type, and template-template pack
      expansions that precede fixed class-template arguments.
- [x] Keep non-final expansion argument lists non-deduced during C++20
      function-template deduction while honoring explicit pack arguments.
- [x] Preserve template-template pack carriers through AST instantiation,
      instance caching, and Itanium function-template mangling.
- [x] Compare GCC C++20 behavior and pass native-Windows
      `test-cxx-function-template-overloads`: i686/AMD64 assembly, AMD64
      execution of RCC output, GCC host execution, and overload diagnostics.
- [x] Encode repeated type, non-type, and template-template class-pack
      expansions with GCC-compatible Itanium substitutions/expressions;
      compare exact GCC/RCC symbol sets for empty and nonempty packs on i686
      and AMD64 and execute the AMD64 output in
      `scripts/ci_nested_class_templates.sh`.
- [x] Pass the complete native-Windows MinGW `test-cxx` aggregate after the
      substitution changes (exit 0).
- [x] Independent function-template pack interactions, including mixed pack
      kinds, are completed in the 2026-10-17 continuation below.
- [ ] Broader C++20 partial-ordering cases remain open.

## RCC continuation (2026-10-17, independent function-template packs)

- [x] Deduce independent `FirstTypes...` and `SecondTypes...` ranges in a
      non-final class-template-id function parameter when another parameter
      supplies the second pack's deduction source. Preserve each range through
      `sizeof...`, specialization caching, instantiation, and mangling; verify
      explicit, empty, and inferred partitions with GCC-identical i686/AMD64
      symbols, AMD64 execution in the POSIX gate, and the native MinGW overload
      regression gate. Also distinguish `1+1`, `2+0`, and `0+2` pack
      partitions that share the same flattened type-argument sequence; runtime
      results and exact GCC/RCC symbols verify the cache key retains boundaries.
- [x] Extend independent partition preservation to non-type and
      template-template packs. Exercise `1+1`, `2+0`, and `0+2` partitions,
      require exact GCC/RCC i686/AMD64 Itanium symbols, and execute the POSIX
      and native MinGW runtime regressions.
- [x] Add a mixed function template with independent type, integer non-type,
      and template-template packs. Exercise uneven offsets and empty/nonempty
      segments; compare exact GCC/RCC i686 and AMD64 symbols, assemble i686,
      and execute the RCC/GCC AMD64 programs in POSIX and native Windows.
- [ ] Broader C++20 partial-ordering cases remain open.

## RCC continuation (2026-10-17, class template-template pack ordering)

- [x] Deduce the fixed leading template-template argument in
      `Box<First, Rest...>` and compare transformed function-template patterns
      for `Box<Templates...>` versus that fixed-prefix form. The all-pack
      overload handles `Box<>`; the fixed-prefix overload wins for one and two
      template arguments. Native-Windows `test-cxx-function-template-overloads`
      passes with GCC host and RCC i686/AMD64 execution. In POSIX, both RCC
      generated programs pass these checks; the enclosing GCC host fixture
      later exits 23 at its conversion-value cleanup assertion, so the full
      POSIX target is not recorded as passing.
- [x] Synchronize the per-template-parameter pack length while deducing
      `sizeof...(Args)` in a trailing function-parameter-pack expansion; the
      native-Windows class-template member-function-template target passes.
- [x] Resolve source parameter-pack names such as `values` to generated
      generic-lambda template packs when substituting `sizeof...`; the native-
      Windows generic-lambda target passes on i686 and AMD64.
- [x] Run the complete native-Windows `test-cxx` aggregate after both fixes
      (exit 0).
- [ ] Broader C++20 partial-ordering cases remain open.

## RCC continuation (2026-10-17, dependent template-template ABI names)

- [x] Distinguish a dependent template-id formed from a template-template
      parameter from a plain type parameter. Emit the parameter substitution
      followed by its nested template arguments, including type-only signatures
      where no non-type argument array exists; guard absent template context.
- [x] Compare GCC/RCC Itanium symbols in the C++ debug template fixture for
      both target widths. `test-debug-info` passes, including exact template
      symbol checks for both single and repeated template-template type
      occurrences, plus relocatable DWARF line/info/location-list/frame checks.
- [x] Run the complete native-Windows `test-cxx` aggregate after the ABI fix
      (exit 0).
- [ ] Broader Itanium substitution cases and C++20 partial-ordering remain
      open. No push or parent RCC gitlink update was made.

## RCC continuation (2026-10-17, TLS ABI and deterministic image goldens)

- [x] Correct the C++ TLS object fixture to expect unmangled global-variable
      names and add a namespaced `thread_local` symbol check for its Itanium
      mangled name, TLS binding, section offset, and initialized value. The
      native-Windows `test-tls` target passes for C/C++, both architectures,
      and linked RIN/RLL outputs.
- [x] Add the missing `rinwire/include` directory to the parent RinRuntime
      dynamic-TLS integration test. The signed i686 and x86_64 runtime RLLs,
      RCC consumer RINs, and strict `rinvalidate` checks pass.
- [x] Regenerate deterministic golden hashes after confirming each of the
      ten C/C++ cases produces identical bytes on two consecutive builds;
      the regular `test-golden-artifacts` check passes for RIN/RLL/DRV and RO.
- [x] Accept unscoped enum values as integral array subscripts while keeping
      scoped-enum subscripts ill-formed; the focused C++ enum-class tests and
      reproducible dual-architecture stage0/stage1/stage2 bootstrap pass.
- [x] Rerun the complete native-Windows `test-ci` aggregate after the golden
      refresh with Clang 18.1.4 (exit 0), including image-format,
      deterministic/golden/property/fuzz, relocation, and stage1-to-stage2
      bootstrap checks.
- [ ] Broader C++20 partial ordering, optimizer profitability, and remaining
      language/ABI gaps remain open. Do not push or change the parent RCC
      gitlink without an explicit request.

## RCC continuation (2026-10-17, array/function decay to bool)

- [x] Rank array/function-to-pointer decay followed by pointer-to-bool as a
      standard conversion for overload arguments; preserve array/function
      expressions for lowering, and materialize the converted `bool` when it
      binds to `const bool&`.
- [x] Regress arrays and function designators for by-value and const-reference
      parameters. GCC C++20 `-Wall -Wextra -Werror`, the focused
      `test-cxx-function-template-overloads` gate, and full native-Windows
      `test-cxx` all pass (exit 0), including i686/AMD64 generation and runtime
      execution.
- [x] Extend the runtime regressions to bind the decayed `bool` temporary to
      `bool&&` from both arrays and function designators. GCC C++20
      `-Werror` and the focused native-Windows overload gate pass, including
      i686/AMD64 generation and execution.
- [x] Reject array/function decay and object/function pointers when the
      resulting temporary `bool` would bind to mutable `bool&`; a dedicated
      invalid fixture checks RCC diagnostics on i686/AMD64 and GCC diagnostics
      in the overload gate.
- [x] Bind object-pointer and function-pointer-to-`bool` conversion results
      to `const bool&` and `bool&&`, with runtime coverage and mutable-reference
      negative checks. The focused overload gate and full native-Windows
      `test-cxx` aggregate pass (exit 0).
- [x] Convert data-member and member-function pointers to `bool` using their
      distinct ABI null representations. Cover implicit and explicit value
      conversion, return/local initialization, `const bool&` and `bool&&`
      binding, true/null values, and rejection for mutable `bool&`. Contextual
      `if`, `!`, `&&`, and `?:` forms use the same ABI-aware conversion. GCC
      C++20, both RCC target architectures, generated host execution, the
      focused overload and invalid-reference gates, and the full native-Windows
      `test-cxx` aggregate pass (exit 0).
- [ ] Other multi-step standard conversions, remaining cv/reference binding
      shapes, and general C++20 conversion-sequence ordering remain open.

## RCC continuation (2026-10-17, dependent template-template pack mangling)

- [x] Mangle dependent function-template template-parameter pack patterns as
      Itanium template parameters rather than placeholder names, while keeping
      repeated-pack substitution indices aligned with GCC.
- [x] Verify the existing repeated non-trailing pack fixture through the full
      POSIX nested-class symbol/assembly/runtime gate and rerun native-Windows
      `test-cxx` (exit 0).
- [ ] Other dependent template-template pack and ABI substitution cases remain
      open.

## RCC continuation (2026-10-17, conversion-function result to bool)

- [x] Complete an implicit conversion-function sequence when its result is an
      object pointer, member pointer, or function reference and the requested
      destination is `bool`; retain the one-user-defined-conversion limit and
      do not admit `nullptr_t` as an implicit `bool` source.
- [x] Exercise by-value, `const bool&`, and `bool&&` destinations, including
      non-null and null object-pointer results, a member-pointer result, and a
      function-reference result. GCC C++20 `-Wall -Wextra -Werror` and the
      focused native-Windows `test-cxx-conversion-operator` gate pass with
      i686/AMD64 generation and runtime execution; the complete native-Windows
      `test-cxx` aggregate also passes (exit 0).
- [ ] Other multi-step standard conversions, remaining cv/reference binding
      shapes, and general C++20 conversion-sequence ordering remain open.

## RCC continuation (2026-10-17, converted pointer references)

- [x] Apply the standard pointer conversion after an implicit conversion
      function returns an object pointer, then bind the resulting target-typed
      temporary to a const pointer reference. Cover `int*` to `const void*`
      for null and non-null values and derived-to-base pointer adjustment.
- [x] Reject binding that conversion result temporary to mutable `int*&` with
      a dedicated negative fixture. GCC C++20 and the focused native-Windows
      conversion-operator gate pass; i686/AMD64 compile and runtime checks and
      the invalid-reference diagnostic/no-output checks pass. Full native-
      Windows `test-cxx` passes (exit 0).
- [ ] Other multi-step standard conversions, remaining cv/reference binding
      shapes, and general C++20 conversion-sequence ordering remain open.

## RCC continuation (2026-10-17, arithmetic conversion references)

- [x] Apply the standard arithmetic conversion after an implicit conversion
      function when binding its result to a reference, and materialize the
      target-typed temporary only when the source and referred-to types differ.
      Preserve direct reference results, including lvalue aliasing.
- [x] Cover `int` to `long` for `const long&` and `long&&`, `float` to `double`
      for `const double&`, direct `operator int&()` binding to const/mutable
      references, and rejection of conversion temporaries for mutable `long&`.
      GCC C++20 `-Wall -Wextra -Werror` and native-Windows
      `test-cxx-conversion-operator` pass with i686/AMD64 generation and runtime;
      the invalid fixture verifies diagnostics and absence of `.ro` on both
      targets.
- [ ] Other multi-step standard conversions, remaining cv/reference binding
      shapes, and general C++20 conversion-sequence ordering remain open.

## RCC continuation (2026-10-17, member-pointer conversion references)

- [x] Complete the standard owner conversion after a conversion function
      returns a data-member pointer, then bind the converted member-pointer
      temporary to a const reference. Route the already-ranked implicit
      conversion through the existing owner-adjustment lowering and preserve
      the null member-pointer sentinel.
- [x] Cover a base-to-derived member-pointer conversion across a nonzero
      multiple-inheritance offset, a null result, and rejection of a converted
      temporary for a mutable member-pointer reference. GCC C++20 and native-
      Windows `test-cxx-conversion-operator` pass with i686/AMD64 generation and
      execution; member-pointer data/function ABI gates also pass. Both target
      diagnostics and absence of `.ro` are checked for the negative fixture.
- [ ] Other multi-step standard conversions, remaining cv/reference binding
      shapes, and general C++20 conversion-sequence ordering remain open.

## RCC continuation (2026-10-17, pointer-to-void reference overload ordering)

- [x] Apply pointer-to-void versus related-class-pointer overload ordering when
      the converted pointer prvalue binds through the same reference form, not
      only for by-value pointer parameters. Preserve the existing partial
      ordering for unrelated class targets and do not compare different
      user-defined conversion functions' trailing sequences.
- [x] Add GCC-confirmed regressions for an `operator Derived*()` result passed
      to competing `const Base* const&` and `const void* const&` overloads,
      including ordinary and function-template overload sets. The focused
      native-Windows `test-cxx-conversion-operator` gate passes with i686/AMD64
      generation and execution; full `test-cxx` passes (exit 0).
- [ ] Other multi-step standard conversions, remaining cv/reference binding
      shapes, and general C++20 conversion-sequence ordering remain open.

## RCC continuation (2026-10-17, member-pointer reference template ordering)

- [x] Compare related destination-owner conversions for pointer-to-member
      results bound to the same reference form after a user-defined conversion.
      Unwrap reference types before applying the existing member-pointer owner
      ranking, while preserving cv and reference-category distinctions.
- [x] Regress `Base::*` conversion results competing for `Derived::* const&`
      and `Leaf::* const&` in function-template overloads. GCC C++20
      `-Wall -Wextra -Werror`, native-Windows `test-cxx-conversion-operator`
      (i686/AMD64 generation and execution), and full `test-cxx` pass (exit 0).
- [ ] Other multi-step standard conversions, remaining cv/reference binding
      shapes, and general C++20 conversion-sequence ordering remain open.

## RCC continuation (2026-10-17, member-pointer reference cv ordering)

- [x] Compare member-pointer reference targets when owner and member cv
      conversions compete: for related owners, prefer the nearer derived
      owner even when the farther target adds member `const`; for one owner,
      prefer the target that does not add member cv qualification.
- [x] Add GCC-confirmed function-template regressions for conversion results
      bound to those reference targets. Native-Windows
      `test-cxx-conversion-operator` passes with i686/AMD64 generation and
      AMD64 runtime execution; its GCC C++20 `-Wall -Wextra -Werror` host
      build and run also pass.
- [ ] Other multi-step standard conversions, remaining cv/reference binding
      shapes, and general C++20 conversion-sequence ordering remain open.

## RCC continuation (2026-10-17, nested array member-pointer qualification)

- [x] Recurse nested-pointee qualification checks through array member
      elements. Reject `int* (C::*)[2]` to `const int* (C::*)[2]`, matching
      GCC C++20, while preserving the legal direct array-member qualification
      conversion.
- [x] Add a GCC-confirmed negative regression for the conversion-function
      result on i686 and AMD64; both diagnostics and absent output artifacts
      are checked. The focused owner-path gate and full native-Windows
      `test-cxx` aggregate pass (exit 0).
- [ ] Other multi-step standard conversions, remaining cv/reference binding
      shapes, and general C++20 conversion-sequence ordering remain open.

## RCC continuation (2026-10-17, nested ordinary-pointer qualification)

- [x] Do not treat a conversion-function result `int**` as identity-compatible
      with `const int** const&`; the nested qualification is unsafe. Reject it
      while preserving the safe conversion to
      `const int* const* const&`.
- [x] Compare both forms with GCC C++20. The native-Windows focused gate checks
      GCC and RCC acceptance/rejection, diagnostics, and absent output for
      i686/AMD64; `test-cxx-conversion-operator` and
      `test-cxx-function-template-overloads` pass.
- [ ] Other multi-step standard conversions, remaining cv/reference binding
      shapes, and general C++20 conversion-sequence ordering remain open.

## RCC continuation (2026-10-17, native Windows CI coverage)

- [x] Add a `windows-latest`/MSYS2 UCRT64 job that builds RCC/RCC++/RLD and
      RinValidate, then runs the full C++ and DWARF regression aggregates
      through native `cmd.exe` recipes.
- [x] Parse-check the workflow and locally verify the native toolchain and
      RinValidate build commands. The exact native Windows `test-cxx` and
      `test-debug-info` gates pass on this committed compiler source revision.
- [ ] Record a successful hosted GitHub Actions run for `native-windows-cmd`;
      local Windows results do not verify the hosted runner setup.

## RCC continuation (2026-10-17, native Windows optimizer gate)

- [x] Add `test-optimize` to `native-windows-cmd` so the MinGW-native job gates
      optimizer output and execution for C and C++ on both target architectures.
- [x] Run `mingw32-make SHELL=cmd.exe test-optimize` locally on native Windows;
      all compile, diagnostic, object-inspection, and execution checks pass.
- [ ] Record a successful hosted GitHub Actions run after this workflow change.

## RCC continuation (2026-10-17, defaults after explicit non-final packs)

- [x] After function-argument deduction processes trailing non-pack parameters
      following an explicitly supplied non-final template parameter pack, apply
      defaults to parameters that remain non-deduced. Keep required parameters
      without defaults non-viable.
- [x] Compare the positive non-deduced `decltype(sizeof(Tail))` suffix with a
      default and the corresponding no-default rejection against GCC C++20.
      Native-Windows `test-cxx-function-template-overloads` passes with i686/
      AMD64 generation and runtime execution; both target diagnostics and absent
      `.ro` output are checked for the negative case.
- [ ] Other function-pack shapes, deduction sources, default-dependency
      interactions, and broader C++20 partial-ordering gaps remain open.

## RCC continuation (2026-10-17, pack-dependent nested default types)

- [x] Preserve `typename ClassTemplate<sizeof...(Pack)>::type` as a dependent
      nested type until the template-id owner is substituted; do not resolve it
      prematurely against the primary class definition.
- [x] Substitute function-template type defaults with the active explicitly
      supplied type/value pack arguments and per-pack offsets/counts. Resolve
      full explicit class specializations after those arguments become concrete.
- [x] Add GCC C++20 coverage for a non-final function-template pack whose
      default type selects `char` for an empty pack and `short` for two elements.
      Native-Windows `test-cxx-function-template-overloads` passes, including
      i686/AMD64 generation, generated AMD64 execution, GCC host execution, and
      the existing negative-diagnostic checks.
- [ ] Deeper nested paths, non-type conversion edge cases, and mixed/multiple
      pack default interactions remain open; see the follow-up below for the
      verified type and non-type partial-specialization cases.

## RCC continuation (2026-10-17, variadic class partial specialization in dependent defaults)

- [x] Register multi-argument type-only and non-type partial specializations
      for variadic class-template primaries, and select them during ordinary
      instantiation and dependent nested-type substitution. Expand a type pack
      through a non-type class-template argument expression after substitution.
- [x] Select a two-argument template-template partial specialization for a
      trailing template-template pack, both directly and after dependent
      nested-type substitution.
- [x] Compare the type, value, and template-template partial specializations
      and empty-pack primary results against GCC C++20; native-Windows
      `test-cxx-function-template-overloads` passes for i686/AMD64 generation,
      generated execution, host execution, and existing diagnostic checks.
- [ ] Other template-template partial-specialization patterns, broader nested
      paths, dependent non-type conversions, and mixed multi-source pack
      patterns remain open.

## RCC continuation (2026-10-17, mixed fixed/pack class partial specializations)

- [x] Parse a fixed leading type followed by a type-pack expansion in class
      partial-specialization patterns, map zero or more actual pack arguments,
      and instantiate the selected specialization with its flattened pack.
- [x] Regress empty, one-element, and multi-element packs, including selection
      through a dependent nested-type substitution. The native-Windows
      `test-cxx-class-template-specialization` gate passes for i686/AMD64
      generation and AMD64 execution; GCC C++20 `-Wall -Wextra -Werror`
      syntax checking passes.
- [x] Reuse an in-progress class instance when its own dependent nested type
      is resolved while cloning member declarations, preventing recursive
      re-instantiation. Native-Windows `test-cxx-class-template-dependent-
      nested-type` passes for i686/AMD64 generation and execution.
- [ ] Multiple and mixed-kind pack expansions, dependent non-type/template-
      template expansions, and broader partial-ordering edge cases between
      pack patterns remain open.

## RCC continuation (2026-10-17, class partial-specialization pack ordering)

- [x] Prefer a pointer-constrained fixed head (`Head*`) over the general fixed
      head (`Head`) when both class partial specializations match and both are
      followed by the same type-pack expansion.
- [x] Check direct empty/general/pointer selection, multi-element pointer-pack
      selection through a dependent nested type, GCC C++20 static assertions,
      and native-Windows `test-cxx-class-template-specialization` for i686/
      AMD64 generation and AMD64 execution.
- [ ] Crossed constraints, unequal expansion structure, and non-type or
      template-template pack partial-ordering cases remain open.

- [x] Add crossed class-template partial-specialization patterns constrained
      by independent named concepts. Verify each singly satisfied constraint
      selects its matching specialization, and that satisfying both leaves the
      structurally incomparable patterns ambiguous. GCC C++20 plus RCC++
      i686/AMD64 ambiguity diagnostics are part of the class-specialization
      regression gates.
- [x] Deduce nested class-template-ids recursively in the unequal pack
      expansions `<Head, UnequalExpansionBox<Tail>...>` and
      `<UnequalExpansionBox<Head>, Tail...>`. GCC and RCC++ i686/AMD64 select
      the primary for an empty pack and each partial specialization for its
      non-overlapping cases; one- and two-element overlaps are diagnosed as
      ambiguous by GCC and RCC++, with no RCC object emitted. Positive
      i686/AMD64 generation and AMD64 execution pass.
- [x] Rank a nested class-template-id above a general type parameter and
      recurse into its type arguments when comparing partial specializations.
      GCC and RCC++ select `Box<T>` over `T`, then `Box<T*>` over `Box<T>`;
      native-Windows `test-cxx-class-template-specialization` passes for
      i686/AMD64 generation and AMD64 execution.

## RCC continuation (2026-10-17, constrained class-pack partial ordering)

- [x] For equivalent class partial-specialization patterns, use associated
      constraint subsumption to prefer the constrained candidate over its
      unconstrained counterpart, in both direct and dependent instantiation.
- [x] Regress equivalent `Head, Tail...` patterns with and without a satisfied
      constraint; compare with GCC C++20. Native-Windows
      `test-cxx-class-template-specialization`,
      `test-cxx-class-template-dependent-nested-type`, and
      `test-cxx-function-template-overloads` pass. Existing ambiguity,
      incomparable-pattern, and unsupported-constraint diagnostics still pass
      for i686/AMD64.
- [x] Verify structural ordering remains stronger than constraint ordering:
      a `Head*` partial beats a constrained general `Head` partial, in direct
      and dependent selection. GCC C++20 and native-Windows
      `test-cxx-class-template-specialization` pass for i686/AMD64 generation
      and AMD64 execution.
- [ ] Constraint ordering across non-equivalent patterns, crossed constraints,
      and non-type/template-template pack patterns remain open.
- [x] Preserve ambiguity between a repeated non-type pattern `X<N, N>` and a
      split parameter/fixed-value pattern `X<M, 5>` when both match `X<5, 5>`.
      GCC C++20 and RCC++ i686/AMD64 diagnose the ambiguity, and RCC emits no
      object file.

## RCC continuation (2026-10-17, function constraint-order parameter guards)

- [x] Require corresponding function-template parameter lists before applying
      constraint subsumption: check arity, parameter kind and packness, and
      mapped non-type parameter types and parser-supported template-template
      signatures, in addition to the function parameters.
- [x] Regress more-constrained overloads whose template parameter lists differ
      by arity, parameter kind, or non-type parameter type. GCC C++20 and
      native-Windows `test-cxx-function-template-overloads` agree on ambiguity;
      i686/AMD64 diagnostics and positive AMD64 overload execution pass.
- [x] Compare `Box<T>` function parameters through corresponding
      template-template parameters before constraint subsumption. The stronger
      associated constraint is selected as with GCC C++20; native-Windows
      `test-cxx-function-template-overloads` passes for i686/AMD64 generation,
      AMD64 runtime, and GCC host runtime.
- [x] Parse a type default in a nested template-template signature and compare
      equivalent constrained overloads whose signatures differ only by that
      default. GCC C++20 and native-Windows `test-cxx-function-template-overloads`
      select the stronger constraint; i686/AMD64 generation and both host
      execution checks pass.
- [x] Parse concept-constrained type parameters in nested template-template
      signatures. Compare constrained and unconstrained nested signatures in
      otherwise corresponding function overloads; GCC C++20 selects the
      stronger outer constraint and native-Windows
      `test-cxx-function-template-overloads` passes for i686/AMD64 generation
      and both execution checks.
- [x] Apply an outer-dependent type default when a `Box<T>` specialization
      omits its trailing nested template argument; compare it with an explicit
      `Box<T, T>` overload against GCC C++20. Native-Windows
      `test-cxx-function-template-overloads` passes for i686/AMD64 generation
      and RCC/GCC host execution.
- [x] Apply an omitted non-type default dependent on an outer type (`sizeof(T)`) in
      a nested template-template signature; compare `Box<T>` with an explicit
      `Box<T, sizeof(T)>` overload against GCC C++20. Native-Windows
      `test-cxx-function-template-overloads` passes for i686/AMD64 generation
      and RCC/GCC host execution.
- [x] Apply an omitted template-template default in a nested signature and
      compare `Box<T>` with an explicit `Box<T, PackOrderTemplateBox>` overload
      against GCC C++20. Native-Windows
      `test-cxx-function-template-overloads` passes for i686/AMD64 generation
      and RCC/GCC host execution.
- [x] Deduce an outer non-type parameter from an omitted nested default that
      names it (`template<typename, int = N>`); compare `Box<T>` against
      explicit `Box<T, N>` overloads. GCC C++20 and the native-Windows
      `test-cxx-function-template-overloads` agree on the more constrained
      overload; i686/AMD64 generation and RCC/GCC host execution pass.
- [x] Defer nested non-type default matching when its compound expression
      (`N + 1`, `2 * N + 1`, and
      `static_cast<int>(3 * N + 2)`) references an outer non-type parameter
      that has not yet been deduced from another function parameter. The
      native-Windows `test-cxx-function-template-overloads` passes for both
      i686 and AMD64 generation, plus RCC/GCC host execution.
- [x] Defer matching a nested dependent member-type default such as
      `typename T::value_type` or
      `typename T::value_type::nested_type` when `T` has not yet been deduced
      from an earlier function parameter. Resolve and compare the nested type
      after another parameter supplies `T`; GCC C++20 and native-Windows
      `test-cxx-function-template-overloads` agree on the selected overload,
      with i686/AMD64 generation and RCC/GCC host execution passing.
- [x] Evaluate a named concept-id inside a conditional dependent non-type
      default in a nested template-template signature, with the outer `N`
      deduced from a later array parameter. Compare overload selection against
      GCC C++20; native-Windows `test-cxx-function-template-overloads` passes
      for i686/AMD64 generation, RCC/GCC host execution, and diagnostic checks.
- [x] Evaluate a unique namespace-scope integer-returning `constexpr` function
      call in a dependent nested template-template non-type default after the
      outer `N` is deduced. Compare the selected constrained overload with GCC
      C++20; native-Windows `test-cxx-function-template-overloads` passes for
      i686/AMD64 generation, RCC/GCC host execution, and diagnostic checks.
- [x] Rank non-template constexpr free-function overload candidates by their
      argument conversion sequences, rather than declaration order; compare an
      exact `int` match against a `long` conversion with GCC C++20. The complete
      native-Windows `test-cxx-function-template-overloads` gate passes for
      i686/AMD64 generation, RCC/GCC host execution, and diagnostic checks.
- [x] Deduce a constexpr function-template parameter from the declared type of
      an enclosing non-type template parameter in a nested template-template
      default, then evaluate its specialization after the outer value is known.
      GCC C++20 and native-Windows `test-cxx-function-template-overloads`
      agree; i686/AMD64 generation, RCC/GCC execution, and diagnostics pass.
- [x] Resolve a constrained versus unconstrained constexpr function-template
      overload inside a dependent nested template-template default, using the
      outer non-type argument's declared type. GCC C++20 and the complete
      native-Windows `test-cxx-function-template-overloads` gate agree across
      i686/AMD64 generation, RCC/GCC execution, and diagnostics.
- [x] Resolve namespace-scope constexpr overloads imported by a named
      using-declaration in a nested template-template non-type default, ranking
      candidates by argument conversion sequences. Native-Windows
      `test-cxx-function-template-overloads` passes for i686/AMD64 generation,
      RCC/GCC host execution, and diagnostics.
- [x] Resolve namespace-scope constexpr overloads introduced by transitive
      using-directives and inline namespaces in a nested template-template
      non-type default. Preserve direct-scope lookup priority and rank imported
      overloads by argument conversions. The native-Windows
      `test-cxx-function-template-overloads` gate passes for i686/AMD64
      generation, RCC/GCC host execution, and its existing diagnostics.
- [x] Resolve enum-argument ADL for a namespace-scope non-template constexpr
      call in a dependent nested template-template non-type default. Defer
      substitution of not-yet-deduced template-template parameters after an
      explicit template-argument prefix; `test-cxx-function-template-overloads`
      passes for i686/AMD64 generation, GCC comparison, generated host
      execution, and existing diagnostics.
- [x] Normalize inline-namespace associated class types to their enclosing
      non-inline namespace before ADL. Verify function-template lookup against
      GCC and generated i686/AMD64 output/execution.
- [x] Resolve enum-argument ADL when the enum is declared in an inline
      namespace and the namespace-scope constexpr function is declared in its
      enclosing namespace. Cover dependent nested template-template defaults,
      plus fully qualified enumerators through the inline namespace, its
      injected enclosing namespace, and a namespace alias as function-template
      non-type arguments;
      native-Windows
      `test-cxx-function-template-overloads` passes for i686/AMD64 generation,
      RCC/GCC execution, GCC comparison, and diagnostic checks.
- [x] Scope parser type-name lookup to the declaring C++ namespace, including
      inline/anonymous namespace injection and namespace using-directives, so
      unrelated same-spelled types do not collide. Verified by native-Windows
      `test-cxx-template-two-phase-adl`, `test-cxx-namespace-extension`, and
      `test-cxx-function-template-overloads`.
- [ ] Broader class/enum ADL and imported function-template overloads remain
      open. Other compound outer non-type forms, deeper paths through dependent
      intermediate aliases, and additional C++20 function partial-ordering
      cases also remain open.

## RCC continuation (2026-10-18, namespace using-directive type ambiguity)

- [x] Resolve C++ unqualified type lookup by lexical scope and namespace lookup
      depth. Diagnose distinct types that meet at the same effective lookup
      level through `using namespace`, including both a same-scope collision
      and a collision lifted to a common enclosing namespace. Preserve a
      nearer block/namespace declaration over an imported or outer type. The
      native-Windows `test-cxx-namespace-extension` target passes for i686 and
      AMD64 with GCC comparison; the full `test-cxx` gate also passes.
- [x] Audit namespace lookup through transitive nested using-directives and
      common-ancestor arrangements. A three-level using-directive chain that
      collides with an enclosing direct type is rejected consistently with
      GCC C++20 on i686 and AMD64 by `test-cxx-namespace-extension`.

## RCC continuation (2026-10-18, dependent function parameter pack shapes)

- [x] Add C++20 coverage for dependent nested-alias function parameter packs
      passed by value and by fixed-bound array lvalue reference. Use two owner
      types whose aliases resolve to different scalar types (`short` and
      `long`) to verify heterogeneous expansion on i686 and AMD64.
- [x] Verify each instantiated formal-parameter DIE refers to its concrete
      scalar type; for array references, verify the reference-to-array DIE,
      element type, and upper bound. GCC and RCC Itanium symbols match exactly.
- [x] Pass native-Windows MinGW GCC `test-debug-info` (exit 0).
- [x] Add a dependent nested-alias function parameter pack of pointers to
      fixed-bound arrays. Verify each formal-parameter DIE points through a
      pointer DIE to the array DIE, retains the bound of two, and resolves the
      heterogeneous element types (`short` and `long`) on i686 and AMD64. Check
      the exact Itanium symbol against GCC and pass `test-debug-info`.
- [x] Cover structural-class non-type parameter packs whose values contain
      `float` and `double` members, including an empty pack. Compare exact GCC
      Itanium symbols, generate i686/AMD64 output, and execute the RCC result
      through `test-cxx-nested-class-template-owner-structural-value-pack`.
- [ ] Other dependent function parameter pack forms, additional structural
      non-type member layouts, and uninstantiated dependent alias/template-
      definition DIEs remain open.

## RCC continuation (2026-10-18, nested structural multi-pack folds)

- [x] Zip an outer structural integer-value pack, a nested scoped-enum
      structural pack, and a member-function type pack in one fold, including
      `sizeof(Elements)` per expansion. Verify empty packs and execute the
      native-Windows RCC/GCC C++20 host tests.
- [x] Preserve the per-element index while a class-template fold is partially
      instantiated, then substitute the member-function type pack at its
      specialization. Diagnose unequal pack lengths; GCC and RCC agree, and
      RCC emits no object for i686 or AMD64 mismatch cases.
- [x] Match GCC's exact Itanium symbol for the nested specialization on both
      target ABIs. Record enclosing class-specialization and nested
      class-template-prefix substitution candidates before structural NTTP
      arguments.
- [ ] Other nested dependent pack sources, additional structural NTTP member
      layouts, and broader ABI substitution interactions remain open.

## RCC continuation (2026-10-18, integral arrays in structural NTTP packs)

- [x] Accept fixed-bound one-dimensional integral and enum arrays as public
      structural-class members. Collect aggregate elements, preserve values
      in both compact and record-backed representations, and substitute
      constant-index member access inside template instantiations.
- [x] Emit GCC-compatible Itanium array-subobject expressions and array-type
      substitutions. Compare the exact GCC/RCC symbol on i686 and AMD64.
- [x] Gate the regression in `test-cxx`; assemble both target outputs and run
      RCC-generated and GCC-generated host executables. The focused test and
      complete native-Windows `test-cxx` aggregate pass.
- [x] Close the pointer-array and multidimensional-array gaps; subsequent
      pointer, member-pointer, and multidimensional structural-array entries
      below record the completed cases.

## RCC continuation (2026-10-18, floating arrays in structural NTTP packs)

- [x] Extend fixed-bound one-dimensional structural-class NTTP arrays to
      `float` and `double`. Preserve their IEEE value bits in compact and
      record-backed storage, constant-index member access, and Itanium
      mangling.
- [x] Compare exact GCC/RCC symbols; assemble i686 and AMD64 output and execute
      RCC/GCC host regressions. The focused target and full native-Windows
      `test-cxx` aggregate pass.
- [x] Close the pointer-array and multidimensional-array gaps; subsequent
      pointer, member-pointer, and multidimensional structural-array entries
      below record the completed cases.

## RCC continuation (2026-10-18, nested structural-class arrays in NTTPs)

- [x] Support fixed-bound arrays of structural classes as structural-class
      NTTP members. Flatten nested aggregate initializers, pack and recover
      member values, substitute constant-index element-member access, and emit
      nested aggregate Itanium expressions.
- [x] Compare exact GCC/RCC symbols, assemble i686 and AMD64 output, and run
      RCC/GCC host tests. The focused regression and full native-Windows
      `test-cxx` aggregate pass.
- [x] Close the pointer-array and multidimensional-array gaps; subsequent
      pointer, member-pointer, and multidimensional structural-array entries
      below record the completed cases.

## RCC continuation (2026-10-18, multidimensional scalar arrays in NTTPs)

- [x] Support fixed-bound multidimensional arrays with integral, enum, float,
      and double scalar leaves in structural-class NTTP members. Recursively
      flatten initialized values, pack them, substitute constant-index paths,
      and emit nested Itanium array aggregates.
- [x] Compare the exact two-dimensional GCC/RCC symbol, assemble i686 and
      AMD64 output, and run the RCC/GCC host regression. The focused target and
      full native-Windows `test-cxx` aggregate pass.
- [x] Close pointer-element arrays and multidimensional structural-class
      element arrays; later continuation entries below record GCC comparisons,
      both target ABIs, and runtime coverage.

## RCC continuation (2026-10-18, native Windows CI re-audit)

- [x] Reconcile the recorded `native-windows-cmd` gate with checked-in
      workflows. The job was absent from the current workflow set despite
      earlier TODO entries; add a dedicated pinned MSYS2 UCRT64 workflow that
      builds RinValidate and runs native `test-cxx`, `test-debug-info`, and
      `test-optimize` using `cmd.exe` recipes.
- [x] Parse the new workflow and rerun its three regression aggregates locally
      on native Windows; all pass. Existing GCC/Clang Linux `test-ci` remains
      the full production gate.
- [ ] Record a successful hosted GitHub Actions run; local Windows execution
      does not verify hosted-runner setup.

## RCC continuation (2026-10-18, verified backend CI gate)

- [x] Add the complete `test-verified-backend` aggregate to the native Windows
      workflow and provision Clang, LLD, and Python `cryptography` for its
      dynamic-TLS RLL build. Parse the updated workflow and confirm the
      generated Make recipe includes the aggregate and Clang invocation.
- [x] Run the complete aggregate locally with the installed Clang path
      explicitly supplied; native-Windows execution exited 0, including the
      dynamic-TLS RLL prerequisite.
- [ ] Verify the hosted Windows job completes the aggregate; a local dry-run
      does not replace execution with the provisioned CI toolchain.

## RCC continuation (2026-10-18, pointer arrays in structural NTTPs)

- [x] Support function- and object-pointer leaves in fixed-bound structural
      class NTTP arrays, including null/mixed-null values and two-dimensional
      pointer arrays. Preserve pointer values through packing, indexed member
      substitution, RTTI identity, and GCC-compatible Itanium mangling.
- [x] Compare exact GCC/RCC symbols, assemble i686 and AMD64 output, and run
      the focused RCC/GCC host regression (`test-cxx-structural-array-nttp`).
- [x] Complete the full native-Windows `test-cxx` aggregate (exit 0).
- [x] Run native-Windows `test-debug-info` (exit 0).
- [x] Member-pointer arrays and multidimensional arrays of structural-class
      elements are covered by the implementation and regression continuations
      below; `test-cxx-structural-array-nttp` passes on native Windows.

## RCC continuation (2026-10-18, member-pointer arrays in structural NTTPs)

- [x] Support fixed-bound arrays of data-member and member-function pointers
      in structural-class NTTPs. Collect member-pointer values without
      treating zero-offset members as null, clone indexed uses, and materialize
      null member-function pointers with their two-word ABI representation.
- [x] Compare GCC/RCC Itanium symbols for non-null and all-null data/member-
      function pointer arrays; assemble i686 and AMD64, and execute the focused
      RCC/GCC host regression (`test-cxx-structural-array-nttp`).
- [x] Re-run the complete native-Windows `test-cxx` aggregate (exit 0).
- [x] Run native-Windows `test-debug-info` (exit 0).
- [x] Cover two-dimensional data-member and member-function pointer arrays,
      including all-null arrays, with exact GCC symbols, i686/AMD64 generation,
      and the full native-Windows `test-cxx` aggregate.
- [x] Support inherited data-member and member-function pointer arrays where
      the public nonvirtual base has a nonzero derived-object offset. Preserve
      adjusted member offsets and `this` adjustments through NTTP collection,
      substitution, and Itanium mangling; verify exact GCC symbols, i686/AMD64
      generation, host execution, full `test-cxx`, and `test-debug-info`.
- [x] Verify that direct virtual-base and base-of-virtual-base member-pointer
      conversions are rejected for data and member-function pointers; compare
      GCC/RCC diagnostics on i686 and AMD64.

## RCC continuation (2026-10-18, multidimensional structural-class arrays in NTTPs)

- [x] Support fixed-bound multidimensional arrays whose elements are
      structural classes. Recursively collect and pack field values for both
      compact values and record-backed values, including nested array layers.
- [x] Substitute constant-index paths through all array dimensions and into
      structural-class members; emit recursive Itanium aggregate values and
      preserve matching structural typeinfo identity.
- [x] Compare exact GCC/RCC Itanium symbols for compact and record-backed
      matrices, generate i686 and AMD64 output, and execute RCC/GCC host tests.
      The focused target and full native-Windows `test-cxx` aggregate pass.
- [x] Run native-Windows `test-debug-info` (exit 0).
- [x] Verify standard-invalid conversions through direct and indirect virtual
      bases remain rejected for data and member-function pointers; compare
      GCC/RCC diagnostics on i686 and AMD64.

## RCC continuation (2026-10-18, bounded local mutable callback inlining)

- [x] Inline automatic mutable function-pointer scalars and fixed-size local
      function-pointer arrays only after proving across the whole function that
      the candidate is neither modified nor escaped. Require non-volatile
      storage, a direct compatible function initializer, and (for arrays) an
      in-range integer constant index.
- [x] Keep reassigned pointers, written array elements, address/reference
      aliases, heterogeneous-target dynamic array indices, and side-effecting
      indices indirect. Check optimized x86 and x64 call shapes and execute the
      AMD64 regressions.
- [x] Pass `test-optimize`, the complete native-Windows `test-cxx` aggregate,
      and `test-debug-info` (exit 0).
- [ ] General alias analysis, mutable callback writes, and aliased or
      cross-function callback target sets remain open. A later bounded
      dispatch item covers proven local fixed-array targets.

## RCC continuation (2026-10-18, uniform-target dynamic local callback arrays)

- [x] Resolve side-effect-free variable-index calls through fixed-bound local
      mutable callback arrays when every explicitly initialized element names
      the same compatible function and whole-function analysis proves the array
      is not modified or escaped. Preserve indirect calls for side-effecting
      indices and arrays whose elements may select different functions.
- [x] Verify optimized i686/AMD64 indirect-call shape, run AMD64 with both valid
      indices, and verify a side-effecting index is evaluated exactly once.
      Native-Windows `test-optimize` passes.
- [ ] General alias analysis, mutable array writes, and callback target sets
      involving aliases or cross-function dataflow remain open. Proven local
      fixed-array heterogeneous dispatch is covered below.

## RCC continuation (2026-10-18, nullptr conversion-sequence rank)

- [x] Classify `nullptr_t` to object, function, and member-pointer targets as
      Conversion rank, not Promotion rank. Add an explicit-template overload
      case where `int*` and `int Owner::*` remain ambiguous; compare against GCC
      C++20 and reject without output on i686/AMD64. The focused
      `test-cxx-function-template-overloads` gate passes.

## RCC continuation (2026-10-18, native Windows production regression gate)

- [x] Run the complete native Windows `test-ci` aggregate with the configured
      Clang toolchain. Exit code 0; C17/C++20, typed IR/MIR, verified backend,
      ABI execution, inline asm, intrinsic headers, image-format goldens,
      property/fuzz checks, and reproducible dual-architecture stage0/stage1/
      stage2 bootstrap all passed.
- [ ] Verify the hosted GitHub Actions Windows job; local execution does not
      establish hosted-runner setup or status.

## RCC continuation (2026-10-18, function-reference conversion decay)

- [x] Complete an implicit conversion function returning `FunctionType&` with
      the standard function-to-pointer decay. Compare with GCC C++20, assemble
      i686 and AMD64 outputs, and execute both RCC targets through
      `test-cxx-conversion-operator`; the full native-Windows `test-cxx`
      aggregate also exits 0.
- [ ] Other multi-step standard conversions, reference-binding shapes, and
      general C++20 conversion-sequence ordering remain open.

## RCC continuation (2026-10-18, array-reference conversion decay)

- [x] Complete an implicit conversion function returning `ArrayType&` with
      array-to-pointer decay, including element qualification and derived-to-
      base pointer adjustment. Compare with GCC C++20 and pass
      `test-cxx-conversion-operator` with i686/AMD64 code generation and native
      execution.
- [x] Run the complete native-Windows `test-cxx` aggregate after this change;
      it exits 0.

## RCC continuation (2026-10-18, user-conversion pointer target ranking)

- [x] Compare the trailing standard conversions after the same conversion
      function when overload targets are `bool` and `void*`. The pointer-to-
      `void*` sequence now wins over pointer-to-bool; GCC agrees, and the
      `test-cxx-conversion-operator` gate passes with i686/AMD64 code generation
      and native execution.
- [x] Compare a single conversion-function result of `Derived*` against
      `void*` and `const void*` overload targets. The unqualified `void*`
      target wins over the sequence that adds pointee qualification; GCC
      C++20, i686/AMD64 generation, and the native execution gate agree.
- [ ] Other multi-step standard conversions, reference-binding shapes, and
      general C++20 conversion-sequence ordering remain open.

## RCC continuation (2026-10-18, C17 generic selection qualifiers)

- [x] Apply C generic-selection controlling-type conversions to top-level
      qualifiers and `_Atomic`, while preserving qualifiers in array decay and
      nested pointer targets. Compare association types with recursively
      qualifier-sensitive compatibility so `int*` and `const int*` remain
      distinct and valid associations.
- [x] Add GCC C17 syntax-oracle and constant-expression regressions for
      qualified scalar controls, top-level-qualified pointer controls, and
      nested pointer qualification; generate i686 and AMD64 `.ro` objects.
      `test-generic` and the full native-Windows `test-c17` aggregate pass.
- [ ] Broader C17 qualifier/effective-type conformance remains open.

## RCC continuation (2026-10-18, generic variably modified associations)

- [x] Reject VLA and pointer-to-VLA types in C17 `_Generic` associations with
      a direct diagnostic. GCC C17 `-pedantic-errors` confirms both are
      constraint violations; `test-generic` checks both diagnostics on i686
      and AMD64 and passes.
- [ ] Broader C17 qualifier/effective-type conformance remains open.

## RCC continuation (2026-10-18, static-reference debug-frame coverage)

- [x] Compile the polymorphic static-reference/subobject regression with `-g`
      for i686 and AMD64 in `test-debug-info`; verify both relevant function
      symbols have relocated `.debug_frame` FDEs with EBP/RBP-based CFA rules.
- [x] Run `make SHELL=cmd.exe test-debug-info`; all relocatable DWARF checks,
      including the new static-reference FDE checks, pass on native Windows.

## RCC continuation (2026-10-18, native-Windows verified-backend gate)

- [x] Add the full `test-verified-backend` aggregate to the native-Windows
      regression job and install the UCRT64 Clang package required by its
      dynamic-TLS runtime integration helper. Keep C++20, DWARF, and optimizer
      gates in the same job.
- [x] Run `test-verified-backend` locally with native `mingw32-make` and Clang
      18.1.4; the complete dynamic-TLS, dual-architecture C/C++ typed-SSA, ABI,
      cleanup, and RinValidate object checks pass.
- [ ] A successful hosted GitHub Actions run remains required to verify the
      UCRT64 package/install and hosted Windows environment.

## RCC continuation (2026-10-18, cleanup for `for`-initializer objects)

- [x] Keep a cleanup-bearing `for` initializer alive across `continue`, emit
      its cleanup on the condition-false edge, and clean it on loop `break` and
      function `return`. Route normal loop completion through a dedicated
      cleanup block so break paths do not destroy the same object twice.
- [x] Preserve i686 wide-scalar SSA state across the cleanup block and merge
      the normally terminated and break edges after cleanup.
- [x] Verify nested-array reverse destruction and exactly-once cleanup for
      normal exit, `continue`, `break`, and `return`, with no verified-backend
      fallback at `-O0`/`-O2` on i686/AMD64. The focused target and complete
      `test-verified-backend`, `test-ir`, and `test-optimize` pass on native
      Windows.
- [ ] Throwing initialization/unwind, partial construction, unsupported VLA
      forms and their interaction with cleanup, `goto` into a protected object
      lifetime, and other cleanup-plan forms remain open. Gotos that leave
      initialized automatic-object scopes are covered by the continuation below.

## RCC continuation (2026-10-18, automatic cleanup on `goto` exits)

- [x] Treat each declaration's complete cleanup plan as one object-lifetime
      entry in sema and typed SSA, matching the legacy backend's cleanup stack.
      Lower validated `goto` exits by emitting the full plan before branching;
      preserve outer-scope cleanups and continue rejecting jumps into a live
      cleanup object's lifetime.
- [x] Verify nested automatic objects with nested-array destructors on goto
      and fallthrough paths, at `-O0`/`-O2` for i686/AMD64 with zero verified
      backend fallback. Also compile and execute the normal x64 backend path.
- [x] Pass `test-cxx-inline-aggregates`, `test-verified-backend`, `test-ir`,
      and `test-optimize` on native Windows. Correct the automatic-cleanup
      fixture's exact emitted-function count in its four architecture/opt
      checks.
- [ ] Throwing cleanup/unwind, partial construction, unsupported VLA forms or
      their interaction with cleanup, and other cleanup-plan shapes remain open.

## RCC continuation (2026-10-18, i686 wide-scalar goto/label state)

- [x] Materialize coherent two-word local mirrors before goto and live
      fallthrough label edges, then resume wide-scalar reads from the stack
      mirror at labels. This avoids using a path-local SSA pair after control
      flow merges at forward or backward labels.
- [x] Add forward-join and backward-loop goto regressions. Require verified
      emission for i686 and AMD64 at `-O0`/`-O2`, then execute the actual
      x86-64 verified `.ro` object to check both branch outcomes and
      loop-carried values through its `main` entry point.
- [x] Implement the formerly open structured-loop goto path and low/high SSA
      predecessor phis at arbitrary labels. The later `test-verified-goto`
      matrix checks 32 functions with zero fallback on i686/AMD64 at `-O0` and
      `-O2`, executes both generated AMD64 objects, and compares the C17 host
      oracle; the detailed loop, join, and phi cases are recorded below.

## RCC continuation (2026-10-18, same-target local callback reassignment)

- [x] Allow scalar local function-pointer inlining when whole-function analysis
      proves every direct reassignment keeps the initializer's function target,
      including conditional writes. Reject escaped pointers and bodies with
      goto/switch entry edges for this proof.
- [x] Keep different-target reassignment indirect; verify optimized i686/AMD64
      call shape and execute the x64 result. `test-optimize` passes on native
      Windows.
- [x] Track direct different-target assignments in straight-line local code
      at each call site; preserve indirect calls at divergent branch joins.
      Verify optimized i686/AMD64 call shape and execute both branch outcomes
      and the sequential reassignment result.
- [x] Merge reaching callback facts across conditional branches when both arms
      assign the same target; retain indirect calls when an arm leaves a
      different target. Verify i686/AMD64 call shape and both x64 outcomes.
- [ ] General local callback dataflow, alias analysis, and indirect-call target
      sets across distinct functions remain open.

## RCC continuation (2026-10-18, const dependent-alias reference packs)

- [x] Expand a function parameter pack of `const typename Owner::value_type&`
      over owners resolving to distinct `short` and `long` types. Verify the
      GCC C++20 Itanium symbol, then check each i686/AMD64 formal-parameter
      DIE follows reference -> const-qualified type -> correct base type.
- [x] Gate the new regression through `test-debug-info`, already part of RCC's
      Linux `test-ci` and native-Windows debug gate.
- [ ] Other dependent pack forms and structural non-integral NTTP packs remain
      open.

## RCC continuation (2026-10-18, floating structural NTTP packs)

- [x] Expand a C++20 structural-class non-type parameter pack containing both
      `float` and `double` members through a fold expression; also verify the
      empty-pack identity. The exact nonempty and empty Itanium symbols match
      GCC, and both i686/AMD64 generated outputs contain them.
- [x] Run the RCC-generated x64 entry point and GCC host oracle. The focused
      `test-cxx-nested-class-template-owner-structural-value-pack` gate exits 0
      and is part of the native-Windows `test-cxx` aggregate.
- [ ] Other dependent pack forms, additional structural member layouts,
      non-finite/extended-precision values, and uninstantiated dependent alias
      or template-definition DIEs remain open.

## RCC continuation (2026-10-18, nullptr arrays in structural NTTP packs)

- [x] Collect and pack fixed-bound one- and two-dimensional `decltype(nullptr)`
      arrays in structural-class NTTP pack values, including class-value records
      whose complete representation exceeds 64 bits.
- [x] Decode array-element member access and emit canonical Itanium structural
      array values; compare nonempty and empty pack symbols byte-for-byte with
      GCC for i686 and AMD64.
- [x] Pass native-Windows
      `test-cxx-nested-class-template-owner-structural-value-pack` and
      `test-cxx-nested-class-templates` (exit 0), including RCC-generated x64
      execution and the GCC host oracle.
- [ ] Other structural member layouts, dependent pack forms, non-finite or
      extended-precision values, and uninstantiated dependent alias/template
      definition DIEs remain open.

## RCC continuation (2026-10-19, bounded heterogeneous callback-array dispatch)

- [x] Lower calls through side-effect-free variable indices of proven,
      unmodified fixed local callback arrays (2–8 fully initialized entries)
      into a bounded comparison chain with direct calls for each target.
      Keep written/escaped arrays, volatile or side-effecting indices,
      side-effecting or non-scalar arguments, and larger arrays indirect.
- [x] Apply the per-caller AST expansion budget and check optimized i686/AMD64
      objects for removal of the indirect call; execute both valid index paths
      on native Windows. `test-optimize` passes (exit 0).
- [ ] General alias analysis, mutable callback writes, and target sets that
      require interprocedural or nonlocal dataflow remain open.

## RCC continuation (2026-10-19, function-pointer parameter optimizer guard)

- [x] Restrict local initializer/reassignment analysis to `DECL_VAR` so
      function-pointer parameters are never read through the unrelated
      `var_init` union member. Keep parameter-based calls indirect and add
      shape plus generated-runtime regressions.
- [x] Reproduce and fix the O1/O2 verified C++ member-function-pointer
      compilation crash. `test-optimize`, `test-cxx-member-pointer-functions`
      (including i686/AMD64 O2 verified objects), and the complete native
      Windows `test-cxx` aggregate all pass (exit 0).

## RCC continuation (2026-10-20, bounded local callback copy chains)

- [x] Follow up to eight initializer or assignment value copies from
      read-only or unmodified local function-pointer variables to a direct
      function target. Keep the proof bounded; this is not general alias or
      interprocedural dataflow.
- [x] Preserve indirect calls when the source address escapes or can be
      changed through an alias. Verify optimized i686/AMD64 call shapes and
      execute direct-copy, two-hop-copy, and alias-write C++ cases in
      `test-optimize` (native Windows, exit 0). Implementations: `15f52b3`,
      `0289893`.
- [ ] Chains beyond the bound, reaching-value propagation for assignment
      copies, aliased callback arrays, and cross-function target propagation
      remain open.

## RCC continuation (2026-10-20, callback copies from reaching local facts)

- [x] At a local function-pointer declaration, copy the source's currently
      proven target even when the source itself has earlier direct assignments.
      Continue using the CFG fact merge: identical branch targets propagate,
      divergent targets do not.
- [x] Verify a direct reassignment followed by a two-hop value copy, and verify
      that divergent branch targets and address-escaped alias writes remain
      indirect. `test-optimize` passes on native Windows with i686/AMD64 call
      shape checks and generated x64 execution. Implementation: `2be0e46`.
- [ ] Compound/conditional assignment-copy forms, callback-array elements,
      and cross-function target sets remain open.

## RCC continuation (2026-10-20, callback facts through pointer assignments)

- [x] Accept direct, type-compatible function-pointer assignment from a local
      pointer variable in the local-target safety proof. At each assignment,
      transfer only the source's current proven CFG fact; unknown or divergent
      source targets clear the destination fact.
- [x] Verify multi-step direct assignments after source reassignment, while
      preserving indirect calls for divergent branch facts and address-escaped
      sources. `test-optimize` passes on native Windows with i686/AMD64 call
      shape checks and generated x64 execution. Implementation: `974f842`.
- [ ] Divergent conditional target sets, callback-array element dataflow,
      alias writes, and cross-function target sets remain open.

## RCC continuation (2026-10-20, uniform conditional callback targets)

- [x] Resolve conditional function-pointer values when both arms have the same
      proven target, including current local CFG facts; retain indirect calls
      for differing targets. Preserve evaluation of a side-effecting condition.
- [x] Verify same-target and divergent-target call shapes for i686/AMD64 and
      execute both divergent arms. The condition side effect occurs exactly
      once in each case; native-Windows `test-optimize` passes. Implementation:
      `61d1273`.
- [ ] General finite target-set dispatch for conditional callback values,
      callback-array element dataflow, alias writes, and cross-function target
      sets remain open.

## RCC continuation (2026-10-19, callback-array alias safety)

- [x] Treat array-to-pointer decay into another local pointer as an escape
      before resolving mutable callback-array targets. Verify an alias write
      changes the selected function result and that optimized i686/AMD64 code
      retains the indirect call; native-Windows `test-optimize` passes.
- [ ] General callback alias analysis, mutable-array dataflow, and target sets
      spanning multiple functions remain open.

## RCC continuation (2026-10-19, wide-scalar goto inside loops)

- [x] Add an i686/AMD64 verified-backend regression where a 64-bit local is
      updated along both sides of a forward `goto`/fallthrough label join
      inside a `while` loop, then carried across the loop backedge. Require all
      five functions to emit without fallback at `-O0`/`-O2`; execute zero- and
      three-iteration cases from both AMD64 generated objects via the host
      object harness. `test-verified-goto` passes.
- [x] Cover a forward `goto` from a nested wide-scalar loop to an outer-loop
      join and a backward `goto` within a nested-loop body that re-enters the
      structured outer-loop backedge. Odd/even and zero-iteration cases pass
      GCC host execution; i686/AMD64 emit without fallback at `-O0`/`-O2`, and
      both AMD64 generated objects execute successfully.
- [x] Cover two distinct `goto` edges and a fallthrough edge carrying different
      64-bit values into one label. All three runtime paths agree with the GCC
      host oracle; i686/AMD64 verified emissions pass at `-O0`/`-O2` without
      fallback, and both AMD64 objects execute successfully.
- [x] Cover a backward `goto` from a nested `while` body to a label immediately
      before that loop, reinitializing the inner bound while carrying a 64-bit
      local. i686/AMD64 verified emissions pass at `-O0`/`-O2` without fallback;
      generated AMD64 execution passes both repeated-restart and zero-inner-bound
      cases in `test-verified-goto`.
- [x] Construct low/high SSA phis for eligible, nonvolatile, non-address-escaped
      i686 wide-scalar locals at arbitrary non-loop labels with goto
      predecessors, including forward multi-predecessor joins and unstructured
      backward-goto loops. Keep edge stack mirrors coherent and verify i686 and
      AMD64 at `-O0`/`-O2`; `test-verified-goto` passes with generated x64
      execution.
- [x] Construct low/high phis for loop-nested labels across forward joins,
      nested-loop exits, backward label cycles, and loop restarts. Synchronize
      wide-local mirrors at `while`/`do-while` conditions, `for` conditions
      and increments, break/continue, and backedges so header loads remain
      iteration-current and loop-local phi values do not escape their
      dominance region. Verify a bounded loop whose condition itself reads
      the loop-carried 64-bit value, detecting stale-header regressions. The
      i686/AMD64 `-O0`/`-O2` verified goto gate checks these phi inputs and the
      generated x64 objects execute successfully.
- [x] Exercise wide-scalar goto joins followed by `continue` and `break` in
      `while`, `for`, and `do-while` loops. Check zero-iteration, early-break,
      continue-to-condition/increment, and carried-value cases against exact
      results; the i686/AMD64 `-O0`/`-O2` verified gate emits all phi inputs and
      both generated x64 optimization levels execute successfully.
- [x] Verify classic C17 codegen restores the dynamic stack when a backward
      `goto` repeatedly exits a VLA scope: a 2048-element VLA is left 5000
      times, with the result checked against GCC. Both i686 and AMD64 generated
      executables pass in `test-vla-runtime`; this target is part of the C17
      production regression gate.
- [x] Lower the supported VLA subset through typed SSA with explicit stack
      save/restore and dynamic stack allocation. `goto` exits restore one or
      multiple nested VLA scopes; normal scope exit and `break`/`continue`
      restore storage in `for`, `while`, and `do` loops. The verified goto gate
      covers partial/full nested-scope exits, repeated backward jumps, and loop
      edges on i686/AMD64 at `-O0`/`-O2`.
- [x] Support recursively fixed-bound array elements under one outer runtime
      bound. Verify `int values[count][2][3]` indexing/stride and dynamic
      allocation on i686/AMD64 at `-O0`/`-O2`, with x64 execution in
      `test-verified-goto`.
- [x] Verify and gate copying plain C aggregates between a fixed local and
      dynamically indexed VLA elements, then mutate one copy independently.
      `test-verified-goto` checks zero-fallback i686/AMD64 objects at `-O0`/`-O2`,
      x64 execution, and a GCC C17 host oracle.
- [ ] Remaining typed-SSA VLA gaps include aggregate initialization/copy forms
      beyond the covered fixed-local, VLA-element, nested-structure, and
      designated-literal cases; atomic operations/types beyond the scalar and
      fixed-size-pointer fetch-add subset; C++ class lifetime hooks; and broader
      cleanup/unwind plus goto/loop-edge combinations.

## RCC continuation (2026-10-19, inherited conversion-function ranking)

- [x] Rank inherited conversion-function candidates using the implied object's
      class and cv/ref binding before comparing result-to-destination
      conversions. Preserve the derived declaration when it hides an inherited
      conversion with the same type.
- [x] Add a positive GCC-checked case where a non-const derived conversion
      wins despite a worse result conversion, plus a distinct-result case that
      must remain ambiguous. The conversion-operator gate passes on i686 and
      AMD64, including native execution.
- [x] Pass the full native-Windows `test-cxx` aggregate (exit 0).
- [x] Prefer a non-template conversion function over an otherwise-equivalent
      conversion-function-template specialization. Add a GCC-checked runtime
      regression; the focused conversion-operator gate and full native-Windows
      `test-cxx` aggregate both pass (exit 0).
- [x] At equal Conversion rank, prefer an integer-to-`bool` result conversion
      over pointer-to-`bool` when comparing distinct conversion functions.
      GCC-checked runtime regression; focused and full native-Windows
      `test-cxx` gates pass (exit 0).
- [x] At equal Conversion rank, prefer `Base* -> void*` over
      `Derived* -> void*` when conversion functions return related class
      pointers. GCC-checked runtime regression; focused and full native-Windows
      `test-cxx` gates pass (exit 0).
- [x] Apply inherited-conversion hiding for a same-target derived declaration
      only when its implicit-object cv/ref signature matches the base
      declaration. Otherwise keep both `using`-exposed ref-qualified
      conversions viable and rank their object bindings; GCC C++20 selects
      inherited `Base::operator int() &&` over `Derived::operator int() const &&`
      for an rvalue. `test-cxx-conversion-operator` and its GCC host oracle pass.
- [x] Rank same-class conversion results that bind a reference by their
      lvalue/rvalue reference category, while preserving derived-to-base result
      ordering when both categories match. GCC C++20 selects the `T&` result
      over `T&&` for `const T&`; the full focused conversion-operator gate passes.
- [ ] Other conversion-sequence ties, ref-qualified inheritance combinations,
      and full C++20 overload-resolution conformance remain open.

## RCC continuation (2026-10-20, plain aggregate typed-SSA VLA elements)

- [x] Permit recursively supported, non-volatile C `struct` and `union` types
      as typed-SSA VLA elements while excluding C++ classes and lifetime hooks
      that need constructor/destructor lowering.
- [x] Add runtime regressions for a VLA of fixed arrays of structures and a
      VLA of unions. Struct/union member access passes i686/AMD64 `-O0`/`-O2`
      verified-backend generation with zero fallbacks and x64 object execution.
      IR, lowering, classic VLA-runtime, and dual-architecture stage0-to-stage2
      bootstrap gates pass. RCC `main` commit: `6b25abe`. No push.
- [ ] Additional VLA aggregate initialization/copy forms, typed-SSA atomic
      operations/types beyond the scalar and fixed-size-pointer fetch-add
      subset, C++ class lifetime lowering, and cleanup/unwind/control-flow
      interactions remain open.
  - [x] Lower supported scalar-width integer `_Atomic` VLA prefix/postfix increment/decrement,
        `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, and `>>=` in
        typed SSA. Implement RMW forms without a direct x86 operation as
        compare-exchange retry loops; cover promoted signed 8/16-bit operands,
        unsigned narrow-result truncation, chained bitwise updates, signed
        division/remainder, and signed/unsigned shifts. Verify both scalar and
        pointer-to-VLA functions emit with zero fallback for i686/AMD64 at
        `-O0`/`-O2`; execute the x64 objects.

## RCC continuation (2026-10-20, typed-SSA multidimensional runtime-bound VLAs)

- [x] Lower VLA declarations with multiple runtime-bound dimensions and mixed
      fixed/runtime dimensions through typed SSA. Evaluate each bound once at
      declaration and retain the computed per-dimension byte extents so later
      indexing keeps the original row stride if a bound variable changes.
- [x] Add a verified-backend runtime regression that mutates the inner bound
      after allocation and then indexes the VLA. i686/AMD64 `-O0`/`-O2`, x64
      execution, IR, IR-lowering, and classic VLA-runtime tests pass.
- [x] Pass the full native-Windows `test-ci` gate, including dual-architecture
      bootstrap reproducibility. RCC `main` commit: `685868d`. No push.
- [ ] Aggregate initialization/copy forms beyond the fixed-local/element
      assignments verified below, typed-SSA atomic operations and additional
      atomic VLA types beyond the scalar and fixed-size-pointer fetch-add
      subset, plus broader cleanup/unwind and control-flow interactions,
      remain open.

## RCC continuation (2026-10-20, volatile scalar typed-SSA VLA elements)

- [x] Permit volatile scalar VLA elements through typed SSA and preserve their
      observable load/store accesses; keep atomic and volatile aggregate forms
      outside this supported subset.
- [x] Add a runtime regression that writes and reads volatile elements at both
      ends of a VLA. The verified-backend i686/AMD64 `-O0`/`-O2` gates emit all
      28 functions with zero fallbacks and x64 execution passes. IR/lowering,
      classic VLA-runtime, and dual-architecture stage0-to-stage2 bootstrap
      gates pass. RCC `main` implementation commit: `8f67474`. No push.
- [ ] Remaining VLA aggregate initialization/copy forms, typed-SSA atomic
      operations/types beyond the scalar and fixed-size-pointer fetch-add
      subset, C++ class lifetime, and cleanup/unwind/control-flow interactions
      remain open.

## RCC continuation (2026-10-20, typed-SSA VLA aggregate copies)

- [x] Verify copying between a fixed-local C aggregate and dynamically indexed
      VLA elements, then mutate the destination and check that the source and
      other element are unchanged. `test-verified-goto` checks zero-fallback
      i686/AMD64 objects at `-O0`/`-O2`, executes both x64 objects, and runs the
      C17 fixture with GCC as an independent host oracle.
- [ ] Additional aggregate initialization/copy forms, typed-SSA atomic
      operations/types beyond the scalar and fixed-size-pointer fetch-add
      subset, C++ class lifetime, and cleanup/unwind/control-flow interactions
      remain open.

## RCC continuation (2026-10-19, nested virtual-base class-array initialization)

- [x] Lower implicit default-member initialization for dynamic and fixed-bound
      class arrays whose virtual-base closure contains nested virtual bases.
      Resolve inherited virtual-base members through the vbtable, use each
      base subobject's non-virtual extent for layout checks, and initialize
      flattened inherited fields only through their owning subobject. Permit
      non-throwing scalar DMI expressions and verify side effects happen once.
- [x] Destroy nested virtual-base closures in reverse closure order without
      recursively destroying a shared virtual base twice. The focused
      `test-verified-cxx-partial-array-init` gate emits i686/AMD64 objects with
      zero typed-SSA fallback and executes dynamic/fixed cases on x64, checking
      DMI call counts, destructor results, and one allocation free per array.
- [x] Exercise a shared-virtual-base diamond with a user-defined `noexcept`
      destructor on the shared root and the complete array element. Dynamic
      `delete[]` now leaves subobject destruction to the element destructor
      body when that body emits its own cleanup, avoiding duplicate virtual-root
      destruction; potentially-throwing destructors retain caller-owned cleanup.
      The regression checks reverse element order and one root destruction per
      element, with fallback-free i686/AMD64 objects and x64 execution in
      `test-verified-cxx-partial-array-init`; the destructor, delete, and
      explicit-throwing-destructor gates also pass.
- [x] Support a user-defined `noexcept` destructor on an intermediate virtual
      base during complete-object cleanup. Invoke its body, emit its
      non-virtual member/base cleanup plan, and leave the shared virtual-base
      closure to the most-derived owner.
- [x] Extend the shared nested-virtual-base array regression with user-defined
      destructors at both intermediate levels. `test-verified-cxx-partial-array-init`
      emits zero-fallback i686/AMD64 objects and verifies x64 reverse element,
      intermediate, and shared-root order (`5,4,3,2,5,4,3,1`). Related
      destructor, scalar-delete, and explicit-throwing-destructor gates pass.
- [ ] Potentially-throwing intermediate virtual-base destructors,
      user-defined subobject constructors beyond the validated subset, and
      throwing DMI construction/unwind remain open.

## RCC continuation (2026-10-19, constexpr conditional result conversions)

- [x] Convert the selected scalar operand of a conditional expression to the
      conditional expression's resolved type during constexpr evaluation.
      A regression where the selected `int` arm is combined with an unselected
      `unsigned int` arm confirms the later comparison uses the required
      unsigned conversion; GCC C++20 agrees, and `test-cxx-constexpr` passes
      for i686 and AMD64 with native x64 execution.

## RCC continuation (2026-10-19, unsigned constexpr call folding)

- [x] Prefer the typed scalar constexpr evaluator when folding integral
      `constexpr` function calls, retaining the older integer evaluator only
      as a fallback when the typed evaluator cannot handle the body. This
      prevents unsigned 64-bit arguments from being compared as signed values.
- [x] Add GCC-checked regressions for `UINT64_MAX < 1ULL` and
      `UINT64_MAX > INT64_MAX`; the calls are folded into `constexpr` variables
      and checked by `static_assert`. `test-cxx-constexpr`,
      `test-cxx-non-type-templates`, `test-cxx-function-templates`, and
      `test-cxx-function-template-overloads` pass on native Windows. Broader
      unsigned constant-expression contexts and structural pointer NTTPs
      remain open.

## RCC continuation (2026-10-19, direct unsigned constexpr expressions)

- [x] Add direct C++20 constant-expression regressions for `UINT64_MAX + 1`
      wrapping to zero, a 63-bit unsigned right shift preserving the top bit,
      and a conditional expression retaining its unsigned result type. GCC
      accepts all three `static_assert`s; `make SHELL=cmd.exe
      test-cxx-constexpr` passes for i686/AMD64 generation and x64 execution.

## RCC continuation (2026-10-19, cv-qualified derived-pointer ranking)

- [x] Rank a derived-to-base pointer conversion ahead of a pointer-to-`void`
      conversion even when only the base-pointer target adds pointee
      qualification. Cover both direct standard conversions and trailing
      standard conversions after the same user-defined conversion function.
      GCC C++20 agrees; `test-cxx-conversion-operator` passes for i686/AMD64
      code generation and native x64 execution.
- [ ] Other conversion-sequence ties, ref-qualified inheritance combinations,
      and full C++20 overload-resolution conformance remain open.

## RCC continuation (2026-10-19, signed constexpr overflow)

- [x] Reject signed overflow using the width of the C++ operation type for
      addition, subtraction, multiplication, division/remainder edge cases,
      left shift, and unary negation. Apply the same checks to both the typed
      evaluator and the legacy evaluator fallback; retain integer promotions
      before compound assignment so a `signed char` increment remains valid.
- [x] Add eight invalid constant-expression cases, including overflow inside
      a called `constexpr` function, and GCC C++20 comparisons. Verify all eight
      RCC diagnostics on i686 and AMD64, plus valid `int` endpoints and signed
      narrow conversion at runtime. `test-cxx-constexpr` and the complete
      native-Windows `test-cxx` aggregate pass.
- [ ] Broader C++20 constant-evaluation coverage, including remaining pointer,
      floating-point, and aggregate edge cases, remains open.

## RCC continuation (2026-10-19, constexpr array-to-pointer decay)

- [x] Evaluate global/static and local array identifiers as pointers to their
      first element in constant-expression contexts. Preserve symbolic pointer
      provenance through pointer initialization, arithmetic, one-past values,
      comparison/subtraction, and constexpr-local array reads.
- [x] Add a GCC-compared regression for mutable global array decay and pointer
      arithmetic plus a constexpr function with a local array. Both target
      assemblies and x64 runtime execution pass; the focused
      `test-cxx-constexpr-pointer` target and complete native-Windows
      `test-cxx` aggregate pass.
- [ ] Remaining constexpr pointer-provenance, floating-point, and aggregate
      edge cases remain open.

## RCC continuation (2026-10-19, constexpr nested-array decay)

- [x] Evaluate array-valued subscript expressions such as `matrix[1]` as
      pointers to the selected row, preserving the global declaration or
      constexpr-local object binding and byte offset.
- [x] Add GCC C++20-checked global and constexpr-local multidimensional-array
      cases for row decay, row indexing, and pointer difference. The focused
      pointer target passes i686/AMD64 generation and x64 execution; the full
      native-Windows `test-cxx` aggregate passes.
- [x] Route constexpr parenthesized pointer-object declarations such as
      `int (*rows)[3]` and `int (*callback)(int)` through the common declarator
      parser rather than the constexpr function parser. GCC C++20 regressions
      cover row pointer arithmetic and a constexpr function pointer; the
      focused pointer target and full native-Windows `test-cxx` pass.
- [ ] Broader constexpr pointer provenance and floating/aggregate cases remain
      open.

## RCC continuation (2026-10-19, inherited conversion object ranking)

- [x] When inherited and same-class conversion functions produce the same
      result type with tied result rank and object cv qualification, prefer
      the same-class function's identity implicit-object conversion over the
      inherited derived-to-base binding. Preserve result-sequence ranking when
      result types differ.
- [x] Add a GCC C++20 regression combining an inherited unqualified
      `const` conversion function with a same-class `const&&` overload. GCC
      selects the derived overload; RCC now agrees. Focused
      `test-cxx-conversion-operator` and the full native-Windows `test-cxx`
      aggregate pass for i686/AMD64 output and x64 execution.
- [ ] Other conversion-sequence ties, ref-qualified inheritance combinations,
      and full C++20 overload-resolution conformance remain open.

## RCC continuation (2026-10-20, zero-valued local constructor operand)

- [x] Treat IR value id `0` as a valid object operand for a direct local
      constructor call, rejecting only `RCC_IR_VALUE_NONE`. The regression
      constructs a scalar-member object whose `noexcept` constructor performs
      arithmetic and assignment in its body.
- [x] `test-verified-cxx-reference-local` passes i686/AMD64 `-O2` verified
      emission with 8 functions and zero fallbacks, executes both verified
      objects, assembles ordinary i686 output, and executes ordinary AMD64 host
      output. Implementation/test commit: `ab380ab`.
- [ ] General constructor bodies, copy construction, full-expression cleanup
      outside call arguments, and exception/unwind interactions beyond the
      direct same-type automatic reference-temporary case remain open.

## RCC continuation (2026-10-20, verified direct-base construction)

- [x] Lower direct non-virtual base constructors before members in declared
      base order when the derived/base constructors are `noexcept`, the direct
      bases have no further inheritance or virtual dispatch, and their ABI
      arguments are supported scalar expressions. Preserve per-base subobject
      offsets and call each real constructor body; retain full-backend routing
      for unsupported lifetime/unwind shapes.
- [x] Add a two-base regression whose constructors mutate shared local state
      and initialize distinct fields, checking declaration-order side effects
      and non-zero secondary-base offset handling. `test-verified-cxx-reference-local`
      passes i686/AMD64 `-O0`/`-O2` with 12 emitted functions, zero fallback,
      verified-object execution, and ordinary backend checks.
- [ ] Virtual/inherited bases, potentially-throwing base constructors,
      partial-construction unwind outside the verified direct class-member
      path for automatic reference-bound prvalues, general constructor bodies,
      and full-expression cleanup outside call arguments remain open. Trivial
      non-virtual base/member copy construction is tracked in the copy section
      below.

## RCC continuation (2026-10-20, verified trivial C++ copy construction)

- [x] Lower sema-approved trivial copy initialization of C++ classes with
      non-virtual bases and scalar, array, and nested struct members in typed
      SSA. Copy subobject values without reading or copying padding bytes.
- [x] Add a local source/destination regression that mutates the source after
      copying. `test-verified-cxx-reference-local` passes i686/AMD64 at `-O0`
      and `-O2` with 13 emitted functions, zero fallback, verified-object
      execution, and ordinary backend checks.
- [ ] Copy construction involving virtual bases, union subobjects, or
      user-defined nontrivial copy constructors, plus general constructor
      bodies and full-expression cleanup outside call arguments, remains open.

## RCC continuation (2026-10-20, pure constructor member expressions)

- [x] Lower side-effect-free scalar member initializers composed of literals,
      constructor parameters, arithmetic/bitwise/comparison/logical operators,
      casts, and conditional expressions.
- [x] Add a two-parameter constructor regression for `first + second` and
      `first * 2 - second`. `test-verified-cxx-reference-local` checks 15
      emitted functions, zero fallback, and execution for i686/AMD64 at
      `-O0`/`-O2` plus ordinary backend output.
- [x] Lower direct resolved `noexcept` calls returning scalar values in scalar
      member initializers when their arguments are supported and do not create
      temporary owners. Emit explicit member initializers in class declaration
      order, regardless of their textual order in the constructor initializer
      list.
- [x] Add a regression whose reversed initializer list calls a side-effecting
      helper for both fields; verify call order and resulting values. The
      focused target passes i686/AMD64 at `-O0`/`-O2`, emits 17 verified
      functions with zero fallback, executes all four verified objects, and
      assembles/runs ordinary backend output.
- [x] Lower comma expressions whose operands are in the supported initializer
      subset, preserving left-to-right evaluation; the regression also checks
      a side-effecting helper call before the comma's final scalar value.
      `test-verified-cxx-reference-local` passes all i686/AMD64 `-O0`/`-O2`
      verified-object and ordinary-backend checks.
- [ ] Potentially-throwing or indirect calls, calls with temporary-producing
      arguments, parameter assignments/increments, non-scalar/class-valued
      member initializers, arbitrary constructor bodies, nontrivial copy
      constructors, and full-expression cleanup beyond supported call
      arguments remain open.

## RCC continuation (2026-10-20, inline floating parameter constants)

- [x] Preserve direct same-type `float`/`double` literal arguments for optimized
      inline formal parameters as their exact IEEE-754 bit patterns, and emit
      them as `DW_AT_const_value` block values linked to abstract parameter DIEs.
- [x] Verify `1.5f` and `2.25` byte-for-byte for legacy and verified optimized
      i686/AMD64 objects. The full `test-debug-info` target passes.
- [ ] Floating conversions beyond the bounded cases tracked below, folded
      expressions, mutable parameter locations, and general optimized
      inline-variable location tracking remain open.

## RCC continuation (2026-10-20, inline floating parameter conversions)

- [x] Extend bounded scalar inlining to insert the parameter's typed cast for
      `float`/`double` argument conversions. Record the converted IEEE-754
      value for direct floating literal arguments, including source-float
      rounding before float-to-double promotion; exclude non-finite and
      out-of-range values from constant attribution.
- [x] Verify runtime conversion results and actual O0-call/O1-inline behavior.
      `test-optimize` and `test-debug-info` validate i686/AMD64, legacy and
      verified optimized DWARF; `1.1f`→`double` and `1.1`→`float` values match
      exact expected bits.
- [ ] Long-double conversions, nonconstant floating expressions beyond
      same-type frame-backed identifiers, dynamic pointer values, symbolic
      addresses outside the supported direct
      global/static-data and function-address forms, additional C/C++ enum
      conversions, nonliteral enum-value attribution, mutable parameter
      locations, and general optimized inline-variable tracking remain open.

## RCC continuation (2026-10-20, out-of-range signed inline values)

- [x] Record converted signed integer parameters using the RinOS i686/AMD64
      modulo-width two's-complement rule, including unsigned values outside the
      signed destination range, without a host out-of-range cast.
- [x] Verify `UINT32_MAX`→`int(-1)` in call-specific legacy/verified DWARF and
      execute `UINT32_MAX`→`int` plus `UINT64_MAX`→`long long` at `-O0`/`-O1`
      for i686/AMD64. Native-Windows `test-optimize` and `test-debug-info` pass.
- [ ] Other implementation-defined integer conversions, nonliteral folded
      arguments, dynamic pointer values and symbolic addresses outside the
      supported direct global/static-data and function-address forms, additional
      C/C++ enum conversions, nonliteral enum-value attribution, mutable
      parameter locations, and general optimized inline-variable tracking
      remain open.

## RCC continuation (2026-10-20, inline floating constant expressions)

- [x] Evaluate bounded floating constant expressions through the semantic
      constexpr scalar evaluator when recording optimized inline parameters.
      Preserve source-type rounding before destination conversion; dynamic,
      non-finite, and out-of-range expressions remain unattributed.
- [x] Verify `1.1f + 2.2f` is rounded as `float` before promotion to `double`
      and compare its exact 8-byte value in legacy/verified optimized i686/AMD64
      DWARF. Native-Windows `test-debug-info` passes.
- [ ] Dynamic floating arguments, long-double support, dynamic pointer values
      and symbolic addresses outside the supported direct global/static-data
      and function-address forms, additional C/C++ enum conversions,
      nonliteral enum-value attribution, mutable parameter locations, and
      optimized inline-variable location tracking remain open.

## RCC continuation (2026-10-20, inline null-pointer parameter values)

- [x] Preserve direct null pointer constants for non-reference, non-volatile
      pointer parameters when recording optimized inline DIEs. Emit
      `DW_AT_const_value` with `DW_FORM_addr` and the target address size;
      this null-constant path does not attribute dynamic locations.
- [x] Add a C inline regression that compares the null argument with a dynamic
      pointer parameter. `test-debug-info` verifies the pointer-typed formal
      parameter DIE and zero address bytes in legacy/verified i686/AMD64 output.
- [ ] Symbolic pointer expressions beyond direct addresses of defined
      global/static data or defined functions, dynamic source expressions
      beyond supported global/static and frame-backed identifiers, and full
      optimized register/piece tracking remain open.

## RCC continuation (2026-10-20, bounded inline enum parameter conversions)

- [x] Admit enum formal parameters to bounded scalar inlining and preserve
      frontend-validated integer/enum argument conversions with a typed cast.
      Keep the existing 1–8-byte, non-volatile, non-reference, non-atomic
      parameter bounds; this does not classify enums as general arithmetic.
- [x] Add runtime coverage for integer-to-enum parameter conversion at `-O0`
      and `-O1`, verify the call is inlined only at `-O1`, and check negative
      and positive enum values on i686/AMD64. Extend inline DWARF checks for a
      C enum formal and `-2` constant in legacy and verified optimized objects.
      Native-Windows `test-optimize` and `test-debug-info` pass.
- [x] Attribute a nonliteral signed C++ enum value held in a `constexpr`
      variable to the inlined enum formal's `DW_AT_const_value`. Verify the
      negative value in legacy and verified optimized i686/AMD64 DWARF;
      native-Windows `make SHELL=cmd.exe test-debug-info` passes.
- [ ] Other C++ enum conversions, nonconstant parameter locations, and
      optimized register/piece tracking remain open.

## RCC continuation (2026-10-20, C++ nullptr inline conversion)

- [x] Include the standard C++ `nullptr_t`-to-object/function-pointer
      conversion in bounded pure-scalar inlining, while excluding member-pointer
      conversion and leaving unrelated pointer conversions unchanged.
- [x] Verify that `nullptr` passed to a pointer formal is inlined at `-O1` but
      remains a call at `-O0`, and that dynamic pointer comparisons execute
      correctly on i686/AMD64. `test-optimize` passes.
- [x] Verify the call-specific pointer formal DIE and zero `DW_FORM_addr` value
      for legacy and verified optimized C++ objects on i686/AMD64.
      Native-Windows `test-debug-info` passes.
- [ ] Symbolic pointer expressions beyond direct addresses of defined
      global/static data or defined functions, dynamic pointer locations, and
      full optimized register/piece tracking remain open.

## RCC continuation (2026-10-20, inline pointer source locations)

- [x] When an inlined pointer parameter receives a same-typed identifier backed
      by a defined global or function-local static object, emit a concrete
      formal-parameter DIE whose `DW_AT_location` points to that object's
      storage. Keep the abstract parameter type/origin and emit the target-sized
      address expression with the corresponding symbol relocation.
- [x] Verify global and function-local static pointer sources in call-specific
      legacy and verified optimized DWARF for i686 and AMD64. The complete
      native-Windows `test-debug-info` target passes.
- [x] Emit frame-relative locations for supported same-typed identifier
      arguments backed by addressable automatic locals or incoming parameters.
      For the verified backend, use its recorded frame slot; otherwise use the
      legacy backend's parameter/local frame offset. Permit bounded
      side-effect-free direct-return and if/else-return trees. Verify the
      parameter DIE/origin, breg expression, and nonzero displacement in legacy
      and verified i686/AMD64 objects, including a pointer local whose address
      escapes after the call.
      Native-Windows `test-debug-info` passes.
- [x] Emit a frame-relative pointer value for a direct `&automatic_object`
      argument to a supported inlined pointer parameter. Append
      `DW_OP_stack_value` so the breg expression describes the pointer value,
      and verify that its displacement matches the source variable's frame
      location in legacy and verified i686/AMD64 objects.
- [x] Preserve the same frame-relative value through representation-preserving
      object-pointer casts with no target adjustment, covering pointee cv
      qualification and an explicit object-pointer-to-`const void*` cast.
      Verify both forms against the source frame slot in legacy and verified
      optimized i686/AMD64 DWARF.
- [x] Preserve a fixed non-virtual derived-to-base pointer adjustment rooted
      at an addressable automatic object. Accumulate the semantic cast
      adjustment and encode it in the frame-relative value expression; the
      regression checks the nonzero base offset (`+4`) in legacy and verified
      optimized i686/AMD64 DWARF.
- [x] Preserve addresses of direct non-bitfield members and fixed-index array
      elements rooted at addressable automatic objects. Accumulate the
      non-virtual member and element byte offsets; regressions verify `+4` for
      a member and `+16` for an array element in legacy and verified optimized
      i686/AMD64 DWARF. Native-Windows `test-debug-info` passes.
- [x] Preserve a dynamic pointer value held in an addressable automatic
      pointer slot for a single constant `+N`/`-N` operation. Read the pointer
      with `DW_OP_deref`, scale the integral offset by the pointee size, and
      emit the computed value with `DW_OP_stack_value`; verify `pointer + 2`
      (`+8` bytes) and `pointer - 1` (`-4` bytes) in legacy and verified
      optimized i686/AMD64 DWARF. Native-Windows `test-debug-info` passes.
- [x] Accumulate bounded chains of constant pointer additions/subtractions
      before reading the frame-backed pointer value. Verify
      `(pointer + 2) - 1` as a net `+4` byte displacement for i686/AMD64 legacy
      and verified optimized DWARF; native-Windows `test-debug-info` passes.
- [ ] Register-only or optimized-away sources, virtual-base/member-pointer
      adjustments, nonconstant offsets, unbounded/mixed-type pointer
      arithmetic chains, nested VLA and pointer-to-pointer/indirect dynamic
      subobject paths,
      compound pointer expressions, mutable-parameter tracking, and optimized
      register/piece tracking remain open; this does not implement general
      variable-location tracking.

## RCC continuation (2026-10-20, inline symbolic pointer constants)

- [x] Emit a concrete inline formal-parameter DIE with `DW_AT_const_value` /
      `DW_FORM_addr` and a target-width relocation when the argument is the
      direct address of a defined non-TLS global or function-local static
      object, a defined function, or a defined function passed by bare
      function-designator decay. Include read-only global data, direct
      `&function` addresses, and representation-preserving object-pointer cast
      chains that need no target adjustment. Preserve the byte addend for a
      literal in-bounds or one-past subscript of a defined static array and a
      direct non-bitfield data member with a fixed offset; exclude virtual-base
      adjustments, member pointers, and dynamic addresses.
- [x] Verify mutable global, read-only global, function-local static object,
      static function addresses, bare function-designator decay, and an
      explicit object-pointer cast to `const void*`, `&array[2]` (byte addend
      8), and `&aggregate.member` (byte addend 4) in legacy and verified
      optimized DWARF for i686/AMD64. Extend bounded scalar inlining to accept
      only type-compatible function-to-pointer decay. The native-Windows
      `test-debug-info` target passes.
- [ ] General pointer arithmetic outside the bounded automatic-frame cases,
      nested paths beyond the tested fixed-size automatic two-dimensional
      arrays, pointer casts requiring target adjustment, VLA/pointer-based
      dynamic subobjects, mutable-parameter tracking, and general
      register/piece location tracking remain open.

## RIN/RLL/DRV fini-array format matrix (2026-10-20)

- [x] Validate C++ `.fini_array` images with the production `rinvalidate`
      for i686 and AMD64 across direct RIN, RLL, and NDRV generation. The
      existing matrix covered RIN on both architectures but only AMD64 RLL/NDRV;
      add the missing i686 library and driver artifacts to
      `test-global-finalizers`. The updated target passes on native Windows.
- [ ] Hardware loader execution of finalizers remains outside host validation.

## RCC continuation (2026-10-20, bounded inline pointer arithmetic in DWARF)

- [x] Record an optimized inline pointer formal as a symbolic address plus
      target-width relocation for statically named array designators combined
      with a nonnegative constant formed from integer/character literals,
      unary minus, `+`/`-`/`*`/`/`/`%`, and integer casts (`array + N` or
      `N + array`), plus `&array[i] ± N` and
      `&global_struct.array[i] ± N` when `i` and `N` are statically known.
      Apply the target integer width and usual arithmetic result type before
      scaling by the pointer element size; retain only displacements within the
      same named array, including one-past.
- [x] Verify exact symbol/addend pairs for `array + (1 + 2)`, `1 + array`,
      array one-past, a cast-narrowed `(unsigned char)260`, mixed signed/unsigned
      `(-2 % 3u)`, `&object + 1`, direct-array `&array[2] - 1`/
      `&array[1] + 2`, and member-array `&global_struct.array[2] - 1`; verify
      out-of-bounds and before-begin results for both array forms are not
      attributed. Check i686 and AMD64 legacy and verified optimized DWARF
      through `test-debug-info` on native Windows.
- [x] Recover the containing row-array type after nested subscript decay for
      optimized symbolic pointer values. Exact relocation tests cover
      `&matrix[1][1] + 1` (addend 20) and the row's one-past address
      `&matrix[1][2] + 1` (addend 24); `&matrix[1][2] + 2` is rejected because
      it crosses the row boundary. Native-Windows `test-debug-info` passes for
      i686/AMD64 legacy and verified optimized DWARF.
- [x] Preserve a fixed non-virtual derived-to-base pointer adjustment when an
      optimized inline pointer argument is rooted at a complete static object.
      Accumulate only sema-provided fixed offsets and require the adjusted
      referent to remain within the complete object. The C++ regression checks
      the exact base relocation addend (`4`) on i686/AMD64 in legacy and
      verified optimized DWARF; native-Windows `test-debug-info` passes.
- [x] Emit an inline pointer value expression for one signed or unsigned
      integral local index applied to a frame-backed pointer local. Load both
      frame slots, sign-extend a narrower signed index with DWARF v4 arithmetic
      shifts, scale by the pointee size, and apply pointer addition or
      subtraction; regressions check both frame locations and the exact
      expression on i686/AMD64 in legacy and verified optimized DWARF.
      Native-Windows `test-debug-info` passes.
- [x] Follow safe integral promotions and widening casts back to the
      frame-backed index storage, including signed `char` promoted to `int`.
      Reject narrowing and signed-to-unsigned conversions whose source range
      cannot be represented without changing the value; verify the one-byte
      signed load, sign extension, and scaled expression in all four target /
      backend configurations.
- [x] Compose up to sixteen frame-backed integer terms in a pointer arithmetic
      chain, retaining left-to-right add/subtract order, each term's signedness
      and width, and its pointer-element scale. Verify
      `(pointer + signed_char_index) - unsigned_int_index`, both frame
      locations, and the exact DWARF expression in legacy and verified
      optimized i686/AMD64 output; native-Windows `test-debug-info` passes.
- [x] Resolve an address-taken integral function parameter as a dynamic index
      from its frame slot. Verify the parameter location and emitted pointer
      expression after regenerating the legacy and verified optimized i686 /
      AMD64 objects; the integrated DWARF verifier passes.
- [x] Compose signed index expressions using frame-backed integer locals and
      integer constants through `+` and `-`, flattening the result into at most
      sixteen scaled terms with checked fixed-displacement accumulation. Verify
      `pointer + (signed_char_index + int_index)` and both frame locations in
      all four target/backend variants with the integrated DWARF verifier.
- [x] Compose unsigned `+` and `-` index expressions when the computed width
      equals the target address width. Verify `pointer + (unsigned_long_a -
      unsigned_long_b)` with both locations and target-width loads on i686 and
      AMD64 in legacy and verified optimized DWARF.
- [x] Preserve `&automatic_aggregate.array[index]` for a one-dimensional,
      fixed-size automatic array or array member with a frame-backed signed
      index. Add the fixed member displacement before the scaled runtime index;
      regressions verify both frame slots, the member offset (`8`), and the
      signed scaling expression on i686/AMD64 in legacy and verified optimized
      DWARF. Native-Windows `test-debug-info` passes.
- [x] Resolve static array address paths with supported integer constant
      expressions using target-width signed conversions and unsigned modular
      arithmetic. Verify `&array[1 + 1]` produces symbol/addend (`8`),
      `&array[((unsigned long long)0 - 1ULL) + 1ULL]` produces addend `0`, and
      out-of-bounds `&array[2 + 3]` is not attributed on i686/AMD64 legacy and
      verified optimized DWARF; native-Windows `test-debug-info` passes.
- [x] Compose same-width unsigned `+`/`-` trees over at least two direct,
      frame-backed locals when their width is narrower than the target address.
      Mask the computed index to its source width before element scaling. The
      `first - second` regression verifies subtraction, the wrap mask, scaling,
      and both source locations on i686/AMD64 in legacy and verified optimized
      DWARF.
- [x] Include same-width unsigned integer literal operands in narrow computed
      `+`/`-` indexes. The AMD64 `index + 1u` regression checks the constant,
      source location, wrap mask, scaling, and final pointer operation in legacy
      and verified optimized DWARF.
- [x] Preserve supported narrow unsigned computations through same-width or
      zero-extending unsigned casts. The AMD64 regression wraps `index + 1u` in
      `static_cast<unsigned long>` and verifies the source-width mask and index
      expression in legacy and verified optimized DWARF.
- [x] Encode address-width unsigned constant indexes above `INT64_MAX` with
      `DW_OP_constu` instead of signed fixed displacement. The AMD64
      `0x8000000000000000ULL` regression verifies target-width scaling and the
      final pointer expression in legacy and verified optimized DWARF.
- [x] Compose nine same-width unsigned frame-backed terms in one computed
      pointer index. Verify every source slot, width-correct arithmetic, and
      element scaling in legacy and verified optimized i686/AMD64 DWARF.
- [x] Extend the bounded computed-index term capacity to 32 and verify a
      17-term frame-backed unsigned expression, every source location, the
      source-width wrap mask, and element scaling in legacy and verified
      optimized i686/AMD64 DWARF.
- [x] Keep bounded inlining from double-charging a single-use formal's
      caller-side expression as newly expanded code. Retain the repeated-use
      and callee-local expansion limits; `test-optimize` preserves large
      callees while `test-debug-info` verifies the 17-term inline location.
- [x] Preserve same-width unsigned frame-backed indexes multiplied by a
      compile-time unsigned constant. Apply the coefficient with source-width
      modular arithmetic before pointee scaling; recognize both multiplication
      nodes and the frontend's shift/add normalization (for example,
      `index * 3u` becomes `(index << 1) + index`). Verify every term and its
      `DW_OP_mul` sequence in legacy and verified optimized i686/AMD64 DWARF.
- [x] Preserve same-width signed frame-backed indexes multiplied by a
      compile-time signed constant, including negative coefficients. Sign-extend
      a narrow source before multiplying and encode the coefficient with
      `DW_OP_consts`; verify positive and negative multipliers and both frame
      slots in legacy and verified optimized i686/AMD64 DWARF. Native-Windows
      `test-debug-info` passes.
- [x] Preserve same-width signed/unsigned casts of frame-backed index locals.
      Read the original frame slot but use the final cast type for extension,
      so signed-to-unsigned conversion zero-extends the stored bits on AMD64.
      Verify i686/AMD64 legacy and verified optimized DWARF; native-Windows
      `make SHELL=cmd.exe test-debug-info` passes.
- [x] Preserve frame-backed narrowing casts when the final index width is no
      wider than the narrowest cast in its chain. On little-endian x86, load
      the final low bytes directly and sign- or zero-extend according to the
      final type. Verify signed and unsigned `int`-to-`short` indexes in legacy
      and verified optimized i686/AMD64 DWARF; native-Windows
      `make SHELL=cmd.exe test-debug-info` passes.
- [x] Flatten a mixed-width AMD64 index with a signed `int` converted to an
      unsigned address-width value and combined with an `unsigned long long`.
      Preserve modulo conversion by sign-extending the signed source to the
      address width before pointer scaling; verify both frame slots in legacy
      and verified optimized DWARF.
- [x] Convert a frame-backed signed `short` to an unsigned `int` index when
      the destination is narrower than the AMD64 address width. Sign-extend
      the source, mask to the destination width before scaling, and use direct
      address-width sign extension on i686. Verify legacy and verified
      optimized DWARF on both targets; the native-Windows integrated DWARF
      verifier passes.
- [x] Preserve frame-backed indexes through one narrowing followed by one
      widening cast: same-signed sign extension, signed-to-unsigned conversion
      with the required AMD64 destination-width mask, and unsigned-to-signed
      zero extension. Verify frame locations and exact scaled pointer
      expressions in legacy and verified optimized i686/AMD64 DWARF; the
      native-Windows integrated debug-info test passes.
- [x] Preserve one or more narrowing casts followed by multiple
      value-preserving widening casts, including same-signedness extension and
      unsigned-to-wider-signed zero extension. Verify `int` -> `short` -> `int`
      -> `long` and `int` -> `short` -> `signed char` -> `short` -> `int` using
      the original frame slot and scaled pointer expression in all four target/
      backend variants; the integrated DWARF verifier passes.
- [x] Preserve a signed narrowing followed by signed-to-unsigned conversion
      and a further unsigned cast. Verify the AMD64 32-bit modulo mask, i686
      address-width sign extension, original frame slot, and scaled pointer
      expression in all four target/backend variants.
- [x] Extend an AMD64 signed narrowing -> unsigned 32-bit modulo conversion ->
      wider signed conversion chain. Verify the 32-bit mask before the
      representable signed 64-bit extension, with the original frame slot and
      scaled expression in legacy and verified DWARF.
- [x] Preserve an AMD64 frame-backed `signed char` index through
      `unsigned int` -> `unsigned short` -> `unsigned long long`, emitting the
      required 32-bit and 16-bit masks in order. Also reject the prior
      narrowest-cast shortcut when it would read beyond the original frame
      slot. Legacy and verified DWARF regressions pass in native-Windows
      `test-debug-info`.
- [x] Preserve a frame-backed signed pointer index divided by a nonzero
      same-typed constant. Reconstruct `DW_OP_div` from the optimizer's
      bias-and-shift form for positive and negative powers of two, and retain
      direct signed division for other constants. Verify divisors `2`, `-2`,
      `3`, and `-3`, source slots, and scaled expressions in legacy and verified
      i686/AMD64 DWARF; the full `test-debug-info` gate passes.
- [ ] Signed results too narrow to represent a masked unsigned range; other
      non-value-preserving conversion sequences;
      mixed-width subexpressions that cannot be safely flattened;
      non-frame-backed operands; constants wider than the target address;
      multiplication beyond same-width unsigned or signed compile-time
      coefficients, and other unhandled index operators; pointer
      chains with more than thirty-two dynamic terms;
      virtual-base/member-
      pointer adjustments, negative displacements not rooted at a statically
      indexed element, nested/VLA/pointer-based dynamic subobject paths,
      non-frame-backed indexes, and optimized register/piece location tracking
      remain open.

## RCC continuation (2026-10-20, typed-SSA reference temporary unwind)

- [x] Lower a direct same-type class prvalue bound to an automatic local
      reference in typed SSA. Materialize the complete object, register its
      destructor after initialization with the active exception frame, and
      unregister then destroy it on normal scope exit. Static/TLS owners and
      virtual-base converted-subobject bindings remain outside this path.
- [x] Add a regression where evaluation of a later call argument throws after
      the reference-bound temporary is initialized. `test-verified-cxx-throwing-temporary`
      emits all twenty-one functions with zero fallback for i686/AMD64 at `-O0`
      and `-O2`. It executes the emitted AMD64 objects at both levels and
      checks direct and nested-conditional exception unwinding, normal cleanup,
      initializer-throw behavior, and constructor-prvalue lifetime; i686
      objects are checked for emission and runtime imports. Earlier
      implementation/test commit: `523a46d`.
- [x] Cover a same-type factory call that throws before returning its prvalue:
      no destructor runs for the incomplete result (`560`), while successful
      initialization is destroyed at scope exit (`66`). Both target
      architectures emit at `-O0` and `-O2` with zero fallback; the AMD64
      objects execute successfully at both optimization levels.
- [x] Unwind successfully constructed class members when a later member
      constructor or the enclosing constructor body throws. Register each
      completed direct member after its constructor returns, then unwind in
      reverse completion order; unregister callbacks on explicit and implicit
      normal constructor returns. Nested `second.inner` coverage verifies a
      later member-constructor throw destroys only `inner` and the earlier
      `first` member (`59031`), an enclosing-body throw destroys
      `second.inner` then `first` (`58231`), and successful construction runs
      exactly one reverse-order cleanup (`2231`) on both explicit and implicit
      return paths. The focused target emits twenty-one functions with zero
      fallback on i686/AMD64 at `-O0`/`-O2` and executes both AMD64 objects.
      Implementation/test commit: `2a7fb2e` (extends the earlier constructor
      prvalue coverage in `d14b5d0`).
- [x] Automatic references bound to direct and nested members of a class
      temporary now keep the complete object alive and project the bound
      subobject address.
- [x] A public conversion function returning a nontrivial class prvalue from
      a named local source can bind to `const T&`; the converted temporary
      survives to scope exit and is destroyed exactly once. The focused target
      emits twenty-one functions with zero fallback on i686/AMD64 at `-O0`/
      `-O2` and runs the AMD64 generated and legacy paths (`65707`).
- [x] Keep the same-type conditional class prvalue passed to
      `read_conditional_lifetime_argument(const T&)` in typed SSA. The x64
      translation-unit gate reports only the expected fallback for
      `LifetimeExtendedDerived(int*, int)`, whose direct base constructor may
      throw; the conditional reader itself is no longer the fallback.

## RCC continuation (2026-10-20, conditional local reference temporaries)

- [x] Permit the existing bounded same-type class-prvalue
      conditional lowering when its selected object is lifetime-extended by an
      automatic local reference. Keep the destructor plan attached to the
      complete selected object, initialize caller-owned object storage, and
      preserve the existing call-argument path.
- [x] Cover direct and nested conditional arms when a later argument throws,
      plus normal local-reference scope exit and a throwing initializer.
      `test-verified-cxx-throwing-temporary` emits all twenty-one functions with
      zero fallback for i686/AMD64 at `-O0` and `-O2`; emitted AMD64 runtime
      checks verify `533`, `534`, `535`, `65`, `5303`, `560`, `66`, `59031`,
      `58231`, and `2231`.
      Implementation/test commits: `a52eac6`, `319cc2d`, `d14b5d0`,
      `2a7fb2e`.
- [x] Lower a same-type nontrivial class-prvalue conditional returned from a
      function when its selected arms are a supported aggregate temporary and
      a plain lvalue identifier with a structurally trivial implicit copy.
      The return object receives the selected value and retains destructor
      ownership at the caller. `test-cxx-aggregate-paren-init` verifies the
      lvalue and temporary arms against GCC, executes the AMD64 output at
      `-O0`/`-O2`, and emits four functions with zero verified-backend fallback
      for i686/AMD64 at both optimization levels on POSIX and native Windows.
- [ ] Other conditional arm forms beyond the verified same-type call-argument
      prvalue and the bounded returned-prvalue form above, converted reference
      bindings, static/TLS lifetime, and other full-expression cleanup remain
      open.

## RCC continuation (2026-10-20, automatic temporary subobject references)

- [x] Extend automatic lifetime for direct and nested member-subobject
      references into class temporaries. Store and destroy the complete
      temporary, while initializing the reference with the address of its
      selected member; preserve normal scope-exit and exception-unwind cleanup.
- [x] Verify `test-verified-cxx-throwing-temporary`: all twenty-one functions emit
      with zero verified-backend fallback on i686/AMD64 at `-O0`/`-O2`; AMD64
      generated-object and legacy-host runs verify early-unwind result `5303`
      and nested-member scope-exit result `65`. Static-subobject reference
      regression targets also pass.
- [x] Run the C++ regression targets through exception cleanup, dynamic casts,
      bit-fields, and global constructors. Verified cleanup emits twelve
      functions with zero fallback on both architectures; the expected count
      was updated to reflect the earlier partial-member-unwind implementation.
      Implementation/test commit: `c604935`.
- [ ] Converted references, static/TLS local lifetime extension, additional
      conditional/subobject expression forms, and remaining full-expression
      cleanup paths remain open.

## RCC continuation (2026-10-20, converted local reference temporaries)

- [x] Cover a public class conversion function returning a nontrivial prvalue
      bound to an automatic `const T&` from a named local source. Verify the
      converted object is alive before scope exit and destroyed once at exit.
      `test-verified-cxx-throwing-temporary` passes with twenty-one functions,
      zero fallback on i686/AMD64 at `-O0`/`-O2`, and AMD64 runtime/legacy
      execution (`65707`). Implementation/test commit: `27deec0`.
- [x] Accept aggregate prvalue initializer expressions in no-throw cleanup
      analysis when each initializer is itself nonthrowing, while retaining the
      restricted proof for explicit constructor calls. Bind a conversion
      result from a temporary receiver to a local `const T&`; the test verifies
      the result survives the full expression and is destroyed at scope exit
      (`65708`). `test-verified-cxx-throwing-temporary` emits all twenty-one
      functions with zero fallback on i686/AMD64 at `-O0`/`-O2`; generated
      AMD64 objects and the legacy host path pass.
- [x] Lower cleanup for a nontrivial/destructible class-prvalue conversion
      receiver. Verify its destructor runs at full-expression end and the
      converted reference-bound object is destroyed later at scope exit.
      `test-verified-cxx-throwing-temporary` emits all twenty-two functions
      with zero fallback on i686/AMD64 at `-O0`/`-O2`; generated AMD64 and
      legacy host execution pass (`65708`).
- [x] Extend conversion-result lifetime coverage to mutable `T&&` and
      `const T&&` locals. Verify each receiver dies at the end of initialization
      while the converted object survives until scope exit (`65710`);
      exception-cleanup verified target still emits twelve functions with zero
      fallback on both architectures.
- [x] Preserve a nontrivial prvalue conversion receiver across a potentially
      throwing conversion call. Register its cleanup before the call, destroy
      the receiver exactly once when conversion throws, and do not destroy the
      incomplete converted result (`7410`). The lifetime-extended reference
      initializer accepts the aggregate call only when its ABI result and
      exception cleanup path are supported. `test-verified-cxx-throwing-temporary`
      emits all twenty-two functions with zero fallback on i686/AMD64 at
      `-O0`/`-O2`; generated AMD64 SSA and legacy host execution pass, as do the
      verified exception-cleanup and function-template-reference regressions.
- [x] Support a nontrivial class-prvalue conditional as a conversion-function
      receiver, selecting either aggregate-initializer arm. Lower x64 SRET
      results into the shared conditional destination and keep the selected
      receiver alive through conversion, then destroy it at full-expression
      end. Both arm checks pass (`65712`); the focused target emits twenty-two
      functions with zero fallback on i686/AMD64 at `-O0`/`-O2`, and generated
      AMD64 SSA, legacy host, verified exception-cleanup, and function-template
      reference regressions pass.
- [x] Support a class-prvalue comma expression as a conversion-function
      receiver. Evaluate its left operand once, materialize the aggregate right
      operand as the receiver, destroy it at full-expression end, and retain
      the converted result through local-reference scope exit. The focused
      target emits twenty-two functions with zero fallback on i686/AMD64 at
      `-O0`/`-O2`; generated AMD64 SSA and legacy host checks pass (`65713`),
      as do verified exception-cleanup and function-template-reference tests.
- [x] Verify recursively nested class-prvalue conditional arms when used as a
      conversion-function receiver. The selected inner arm is destroyed once
      at full-expression end; unselected arms are not evaluated, and the
      converted result survives to scope exit. The focused target emits
      twenty-two functions with zero fallback on i686/AMD64 at `-O0`/`-O2`;
      generated AMD64 SSA and legacy host execution pass (`65714`).
- [x] Verify class lvalue- and rvalue-reference conversion results from
      nontrivial temporary receivers passed directly to reference parameters.
      The referenced object remains the original object, and event order `12`
      confirms conversion runs before receiver destruction at full-expression
      end. `test-cxx-function-template-references` passes GCC C++20 diagnostics,
      i686/AMD64 generation, and AMD64 host execution.
- [x] Verify exception cleanup for a `T&` conversion function invoked on a
      nontrivial temporary receiver during reference initialization. When the
      conversion throws, destroy the receiver exactly once before the handler;
      do not destroy the existing referent (`7630`). The focused verified
      target emits twenty-four functions with zero fallback on i686/AMD64 at
      `-O0`/`-O2`; generated AMD64 SSA and legacy host execution pass.
- [x] Preserve the complete nontrivial class-prvalue returned by a user-defined
      conversion when an automatic reference binds to its public virtual-base
      subobject. Project from the fully initialized complete-object owner with
      validated virtual-base/referent bounds, and destroy the derived object
      before its virtual base exactly once (`21`). The thirty-eight-function
      `test-verified-cxx-throwing-temporary` gate passes without fallback on
      i686/AMD64 at `-O0`/`-O2`, with typed-SSA and legacy-host AMD64 execution.
      The legacy-host recipe now defines `RINOS_HOST_RUNTIME`, avoiding a
      conflicting `_start` when linked with the host CRT.
- [ ] Additional conditional/projection combinations, broader static/TLS
      lifetime extension, and remaining full-expression cleanup paths remain
      open.

## RCC continuation (2026-10-20, converted-result base-reference lifetime)

- [x] Preserve the complete nontrivial class-prvalue returned by a user-defined
      conversion when an automatic reference binds to its unique public
      non-virtual base subobject. Retain complete-object cleanup and apply the
      semantically selected base displacement from the initialized owner;
      the regression uses a nonzero-offset base in multiple inheritance and
      observes base value `35` plus exactly-once derived/base destruction order
      `21`.
- [x] `test-verified-cxx-throwing-temporary` emits all thirty-four functions
      with zero fallback on i686/AMD64 at `-O0`/`-O2`; the typed-SSA AMD64
      objects and the AMD64 legacy-host object pass. The change remains limited
      to non-virtual converted-base bindings; the separate virtual-base
      converted-reference continuation below covers that additional shape.

## RCC continuation (2026-10-20, constexpr inline integer parameter values)

- [x] Use the semantic constexpr scalar evaluator when recording optimized
      inline integer parameters, preserving the evaluated target-width bit
      pattern and signedness. This covers constexpr identifiers and bounded
      integer expressions, while leaving non-constant inputs unattributed.
- [x] Verify a constexpr-backed arithmetic expression and a constexpr
      function result as `DW_AT_const_value` on inlined C++ formal parameters
      for i686/AMD64. Native-Windows `make SHELL=cmd.exe test-debug-info` passes.
- [ ] Dynamic integer expressions, general mutable-parameter locations, and
      optimized register/piece tracking remain open.

## RCC continuation (2026-10-20, member-pointer qualification subsequence)

- [x] Verify proper-subsequence ordering when converting `int Root::*` to
      competing `int Middle::*` and `const int Middle::*` parameters. Both the
      function-template and ordinary overload sets select the unqualified
      member target, matching GCC C++20. `test-cxx-function-template-overloads`
      passes i686/AMD64 generation and host execution plus GCC C++20 checks.
- [ ] General standard-conversion subsequences, other member-pointer ranks,
      reference bindings, and user-defined conversion ties remain open.

## RCC continuation (2026-10-20, nested data-member pointer qualification)

- [x] Parse data-member pointer declarators whose member type includes pointer
      layers, distinguishing `int* Owner::*` from pointers to the completed
      member-pointer type. Preserve nested cv in Itanium names for
      `int* const Owner::*` and `int** const Owner::*`, so ordinary overloads
      retain distinct symbols and exact-match selection agrees with GCC C++20.
      The focused `test-cxx-function-template-overloads` gate passes both
      target builds, generated x86/AMD64 execution, and the GCC host oracle.
- [ ] Other nested member types, member-pointer owner adjustments, and general
      C++20 conversion-sequence ordering remain open.

## RCC continuation (2026-10-20, array-to-pointer qualification ranking)

- [x] Rank array-to-pointer decay with added pointee cv-qualification as an
      Exact Match sequence, ahead of decay followed by pointer-to-void
      conversion. An `int[N]` argument now selects `const int*` over
      `const void*`, matching GCC C++20; mutable and const arrays are covered.
      `make SHELL=cmd.exe test-cxx-conversion-operator` and the complete
      `make SHELL=cmd.exe test-cxx` aggregate pass, including i686/AMD64 output,
      generated AMD64 execution, and GCC host execution.
- [ ] General standard-conversion subsequences, reference bindings, other
      pointer-to-member ranks, and user-defined conversion ties remain open.

## RCC continuation (2026-10-20, standard pointer conversion references)

- [x] Apply standard pointer conversions before binding a temporary to a
      reference, including `int*` to `const void* const&`, array decay, and
      derived-to-base pointer adjustment. Preserve direct reference binding
      when a user-defined conversion already returns a compatible lvalue.
- [x] Rank the competing `const int* const&` and `const void* const&`
      sequences so the better pointer conversion wins, matching GCC C++20.
      `make SHELL=cmd.exe test-cxx-conversion-operator` passes GCC checks,
      i686/AMD64 generation, AMD64 execution, and invalid mutable-reference
      diagnostics on both targets. The complete native-Windows
      `make SHELL=cmd.exe test-cxx` aggregate also passes.
- [x] Cover const rvalue-reference materialization for `int*` to
      `const void*`, array decay to `const void*`, and derived-to-base pointer
      adjustment. The `const Base* const&&` overload wins over
      `const void* const&&`, matching GCC C++20; the focused target passes
      generation and AMD64 host execution.
- [ ] Other standard-conversion subsequences, reference-binding categories,
      pointer-to-member ranks, and user-defined conversion ties remain open.

## RCC continuation (2026-10-20, nested typed-SSA VLA aggregate copies)

- [x] Verify aggregate copies between fixed locals and VLA elements when the
      structure contains a fixed-size array and recursively nested structures.
      Also copy a designated compound literal into a VLA element, then copy
      between VLA elements and mutate each side independently. The verified
      backend emits all 31 fixture functions with zero fallback for i686/AMD64
      at `-O0`/`-O2`; both AMD64 objects execute and the GCC C17 oracle passes
      via `make SHELL=cmd.exe test-verified-goto`.
- [ ] Broader VLA aggregate initialization/copy semantics, typed-SSA atomic
      operations/types beyond the scalar and fixed-size-pointer fetch-add
      subset, C++ class lifetime, and cleanup/unwind/control-flow interactions
      remain open.

## RCC continuation (2026-10-20, seq_cst atomic integer VLA access)

- [x] Admit `_Atomic int` VLA elements through typed SSA for direct C lvalue
      loads and stores. Preserve each access through optimization, and encode
      sequentially consistent stores as memory `XCHG`; do not route this case
      through legacy code generation. `test-verified-goto` emits all 32
      functions with zero fallback for i686/AMD64 at `-O0`/`-O2`, executes both
      AMD64 objects, and agrees with the GCC C17 oracle. `test-ir` verifies an
      actual memory-XCHG encoding for both x86 targets and executes the x64
      store. No push.
- [x] Add a typed-SSA atomic integer fetch-add operation returning the old
      value, and encode it as `LOCK XADD`. Lower atomic integer `+=`, `-=`, and
      prefix/postfix `++`/`--` for 8/16/32-bit objects on i686/AMD64 and 64-bit
      objects on AMD64. Also lower atomic pointer `+=`, `-=`, and prefix/postfix
      `++`/`--` when the pointee has a fixed, complete size; dynamic VLA strides
      remain on the legacy path. The encoder uses the byte-specific opcode and
      the 16-bit operand-size prefix; the allocator reserves the XADD/address
      scratch registers, and optimization preserves the memory side effect.
      `test-ir` validates and executes 8/16/32/64-bit integer and pointer
      encodings, including i686 selection. `test-verified-atomic-fetch-add`
      compiles narrow integer and pointer operations at `-O0`/`-O2` with zero
      fallback on both architectures, executes AMD64 outputs, and compares the
      C17 fixture with GCC.
- [x] Preserve unsupported atomic RMW operations such as `*=` through the
      existing atomic-capable legacy lowering. `test-verified-atomic-vla-rmw`
      checks exactly one fallback for i686/AMD64 at `-O0`/`-O2` and executes
      both AMD64 objects; it remains part of `test-verified-backend` and
      production `test-ci`.
- [x] Typed-SSA atomic pointer RMW now uses captured extents for dynamic VLA
      pointees; integer `__atomic_fetch_add/sub` and `__atomic_add/sub_fetch`
      builtins plus scalar `__atomic_exchange_n` are lowered directly, with
      their explicit-order expressions evaluated once. See the continuations
      below.
- [x] Typed-SSA integer atomic RMW beyond fetch-add/sub now lowers bitwise
      fetch-and/or/xor/nand, signed/unsigned fetch-min/max, and unsigned
      fetch-uinc/udec through an observable-load/CAS retry loop. The gate
      checks zero fallback for i686/AMD64 at `-O0`/`-O2`, executes AMD64
      objects, and compares the C17 source with GCC via
      `make SHELL=cmd.exe test-verified-atomic-fetch-add`.
- [x] Lower generic aggregate load/store/exchange/compare-exchange for
      complete naturally aligned 1/2/4/8-byte struct and union objects using
      their integer object representation; require trivial copyability for
      C++ classes and restrict 8-byte objects to AMD64. The focused gate
      verifies i686/AMD64 at `-O0`/`-O2`, runs AMD64 outputs, and compares the
      C17 fixture with GCC.
- [ ] Remaining aggregate sizes/layouts and non-trivial C++ objects, floating
      RMW, and complete C++ atomic API/concurrency semantics remain open.
      Scalar and bounded aggregate generic load/store/exchange/compare-exchange
      plus floating load/store/exchange/compare-exchange have dedicated tests.
- [x] Fix optimized VLA-bound dead-store elimination: runtime array
      declarator bounds and variably-modified types in `sizeof` are now reads
      (including nested pointer/array types), so an initializer such as
      `int count = 3;` is retained when a VLA declaration or `sizeof(int[count])`
      uses it. Escape analysis also traverses evaluated VLA type bounds. The
      focused verified-backend regression executes at AMD64 `-O0`/`-O1`/`-O2`
      with zero fallback; the legacy `sizeof` regression executes at `-O1` and
      `-O2`. `test-verified-vla-local-bound`, `test-verified-atomic-vla-rmw`,
      `test-optimize`, and `test-ir` pass on native Windows.

## RCC continuation (2026-10-21, pointer-to-VLA increment strides)

- [x] Permit prefix/postfix increment and decrement for pointers to complete
      VLA element types. Legacy i686/AMD64 lowering uses the declaration's
      saved runtime extent for both atomic and non-atomic pointers; the VLA
      atomic regression covers postfix increment and prefix decrement results.
- [x] `test-verified-atomic-vla-rmw` compiles i686/AMD64 at `-O0`/`-O2` and
      executes the AMD64 objects at both optimization levels. The tested VLA
      function changes the bound variable after pointer declaration to verify
      the saved stride, and uses the complete legacy backend through the
      verified backend's existing fallback; the gate checks fallback count and
      execution rather than claiming typed-SSA coverage.
- [x] Typed-SSA lowering for the covered dynamic VLA pointer strides is
      implemented for local objects and formal parameters, including pointer
      subtraction, as recorded in the continuations below.
- [x] Extend typed-SSA explicit-order scalar integer atomics beyond fetch
      arithmetic: bitwise/NAND/min/max/wraparound RMW builtins use CAS retry
      loops, evaluate each explicit order expression once, and have zero-
      fallback i686/AMD64 `-O0`/`-O2` compile checks plus AMD64 execution and a
      GCC C17 oracle in `test-verified-atomic-fetch-add`.
- [ ] Additional VLA pointer forms beyond the covered offsets/subtraction,
      aggregate generic atomic builtins and operations, floating-point RMW,
      and complete C++ atomic operations remain open. Scalar generic/floating
      load/store/exchange/compare-exchange, integer RMW, test-and-set/clear,
      and fences have dedicated coverage.

## RCC continuation (2026-10-21, VLA pointer offsets)

- [x] Support pointer `p + n`, `p - n`, `p += n`, and `p -= n` when `p` points
      to a complete VLA object. C semantic checks now accept these complete
      variably-modified element types; both i686 and AMD64 legacy code generators
      scale by the declaration-captured extent, including atomic pointer
      compound assignments. Array-expression decay and commuted `n + p` are
      covered too.
- [x] Extend `test-verified-atomic-vla-rmw` to exercise plain and atomic VLA
      pointers, both addition orders, subtraction, compound updates, and a
      changed bound variable. Compilation/fallback counts pass for i686/AMD64
      at `-O0`/`-O2`; both AMD64 objects execute successfully and the same C17
      fixture passes GCC's native oracle.
- [x] Typed-SSA lowering for dynamic VLA pointer strides and subtraction of
      two pointers to compatible complete VLA objects is implemented in the
      continuation below.
- [x] Typed-SSA lowering for bounded generic aggregate load/store/exchange/
      compare-exchange is complete; see the 2026-10-30 continuation below.
- [ ] Remaining aggregate sizes/layouts, floating-point RMW, and complete
      C++ atomic operations remain open. Scalar generic and floating
      load/store/exchange/compare-exchange plus integral bitwise, min/max, and
      wraparound RMW are implemented and tested.

## RCC continuation (2026-10-21, typed-SSA pointer-to-VLA extents)

- [x] Capture the runtime byte extent at local pointer-to-VLA declaration in
      typed SSA. Plain pointer addition/subtraction, commuted addition,
      compound updates, and atomic prefix/postfix/compound updates use that
      captured stride after the source bound changes.
- [x] Track variably-modified pointer declarations in scope-exit restoration
      and goto analysis without treating extent-only entries as dynamic stack
      allocations. Reject goto into a variably-modified pointer scope and
      preserve valid goto exits from that scope.
- [x] Regressions compare exact byte offsets after changing the bound and
      cover pointer-scope goto. `test-verified-vla-local-bound` emits all five
      functions with zero fallback for i686/AMD64 at `-O0`/`-O1`/`-O2` and
      executes AMD64 objects at all three levels. `test-vla-semantics`,
      `test-verified-atomic-vla-rmw`, `test-verified-goto`, and both GCC C17
      fixture oracles pass.
- [x] Typed-SSA VLA pointer strides for local objects and formal parameters,
      plus subtraction between pointers to compatible complete VLA objects,
      are implemented in the subsequent continuation entries below.
- [ ] Typed-SSA lowering for other explicit-order atomic builtins and RMW
      operators beyond integer fetch arithmetic and scalar exchange,
      floating/aggregate atomics, and general C++ atomics remains open.
- [x] Verify every target in the complete non-hardware `TEST_CI_TARGETS` set.
      The initial `make test-ci` run passed C17, C++20, AQC, IR, and IR lowering,
      then stopped when its restricted PATH omitted the installed Clang needed
      by `test-rinvalidate-dynamic-tls`. Reran `test-verified-backend` and all
      subsequent targets with Clang on PATH; the signed dynamic-TLS RLL,
      `rinvalidate`, fuzz/property gates, and dual-architecture stage0-to-stage2
      bootstrap all passed. Target-set coverage is complete across these runs;
      a single uninterrupted aggregate invocation was not repeated.

## RCC continuation (2026-10-21, typed-SSA VLA pointer parameters)

- [x] Capture pointer-to-VLA parameter extents after lowering their bound
      parameters, before entering the function body. VLA pointer owner lookup
      now includes formal parameters, so indexing and pointer arithmetic use
      the entry-time stride even after a bound parameter is modified.
- [x] Extend `test-verified-vla-local-bound` with a pointer-to-VLA parameter
      regression that mutates the bound, performs pointer arithmetic and
      subtraction, then indexes the pointee. Six functions compile with zero
      fallback for i686/AMD64 at `-O0`/`-O1`/`-O2`; AMD64 verified objects run at
      all levels. Legacy objects compile for both targets and the AMD64 object
      executes; the GCC C17 oracle passes. VLA semantic, IR lowering, atomic
      VLA RMW, and goto gates pass as well.
- [x] Support subtraction of pointers to compatible, complete VLA objects.
      Semantic analysis accepts runtime-complete VLA element types; typed SSA
      divides the byte difference by the left operand's captured extent, and
      legacy i686/AMD64 codegen uses the saved stride too.
- [ ] Typed-SSA lowering for other explicit-order atomic builtins and RMW
      operators beyond integer fetch arithmetic and scalar exchange,
      floating/aggregate atomics, and general C++ atomics remains open.

## RCC continuation (2026-10-21, typed-SSA integer atomic RMW)

- [x] Lower integer `__atomic_fetch_add`, `__atomic_fetch_sub`,
      `__atomic_add_fetch`, and `__atomic_sub_fetch` directly to typed SSA.
      Runtime order expressions are evaluated exactly once; `LOCK XADD` gives
      sequentially consistent ordering, which is stronger than every valid
      order and preserves the requested semantics.
- [x] `test-verified-atomic-fetch-add` checks signed/unsigned 8/16/32-bit
      objects on i686 and AMD64, plus 64-bit objects on AMD64, with zero
      fallback at `-O0`/`-O2`. It exercises all four builtin result forms and
      volatile runtime order expressions; AMD64 objects execute and the GCC
      C17 oracle agrees.
- [x] Lower scalar integer/pointer `__atomic_exchange_n` to typed SSA and x86
      memory-XCHG. Tests cover 8/16/32-bit exchanges on i686/AMD64 and 64-bit
      plus pointer exchanges on AMD64, with zero fallback at `-O0`/`-O2`.
      AMD64 verified objects execute; `test-ir` verifies and executes the
      exchange opcode for both targets.
- [ ] Other atomic integer RMW builtins,
      generic/aggregate atomic builtin forms, floating/aggregate atomics, and
      general C++ atomic operations remain open.

## RCC continuation (2026-10-21, typed-SSA atomic load/store builtins)

- [x] Lower scalar `__atomic_load_n` and `__atomic_store_n` calls directly to
      typed SSA for 8/16/32-bit integers on i686/AMD64, 64-bit integers on
      AMD64, and target-width pointers. Runtime order expressions are evaluated
      exactly once; loads remain observable and stores use memory `XCHG` for
      sequential consistency, stronger than every valid store order. Unsupported
      widths continue through the existing complete legacy backend.
- [x] `test-verified-atomic-fetch-add` covers integer and pointer load/store
      with side-effecting runtime order expressions, zero verified-backend
      fallback at `-O0`/`-O2` on both architectures, AMD64 object execution, and
      a GCC C17 oracle. `test-atomic-builtins`, `test-atomic-language`, and
      `test-ir` pass.
- [ ] Other atomic integer RMW builtins,
      generic/aggregate builtin forms, floating/aggregate atomics, and general
      C++ atomic operations remain open.

## RCC continuation (2026-10-21, typed-SSA atomic pointer fetch arithmetic)

- [x] Lower pointer-valued `__atomic_fetch_add`, `__atomic_fetch_sub`,
      `__atomic_add_fetch`, and `__atomic_sub_fetch` directly to typed SSA.
      Match GCC's builtin byte-offset semantics (no element-size scaling), use
      target pointer width, and preserve old/new pointer results. Runtime memory
      order expressions are evaluated once; unsupported target widths continue
      through the existing complete legacy backend.
- [x] `test-verified-atomic-fetch-add` exercises all four forms on `void*` to
      distinguish byte offsets from ordinary C pointer arithmetic, compiles
      i686/AMD64 at `-O0`/`-O2` with zero fallback, executes AMD64 outputs, and
      agrees with the GCC C17 oracle. Atomic builtin, language, and IR suites
      pass; invalid pointer offset types receive a diagnostic.
- [ ] Other atomic integer RMW operators, generic/aggregate builtin forms,
      floating/aggregate atomics, and general C++ atomic operations remain open.

## RCC continuation (2026-10-21, typed-SSA compare-exchange)

- [x] Lower scalar/pointer `__atomic_compare_exchange_n` calls to typed SSA
      and x86 `LOCK CMPXCHG`. Strong hardware semantics correctly implement
      weak requests; the locked operation supplies sequential consistency for
      every valid success/failure order. Object, expected, desired, weak, and
      order expressions are evaluated once before the expected value is read;
      `*expected` is updated only after failure.
- [x] `test-verified-atomic-fetch-add` covers failed and successful byte
      comparisons, 16/32-bit values, target-width pointers, and AMD64 64-bit
      values. i686/AMD64 `-O0`/`-O2` builds have zero verified-backend fallback;
      AMD64 objects execute and agree with the GCC C17 oracle. Atomic builtin,
      language, and IR/MIR/x86 encoding suites pass.
- [ ] Other atomic integer RMW operators, generic/aggregate builtin forms,
      floating/aggregate atomics, and general C++ atomic operations remain open.

## RCC continuation (2026-10-21, typed-SSA bitwise atomic RMW)

- [x] Lower `__atomic_fetch_and`, `__atomic_fetch_or`, `__atomic_fetch_xor`,
      `__atomic_and_fetch`, `__atomic_or_fetch`, and `__atomic_xor_fetch` with
      typed-SSA compare-exchange retry loops. Preserve old/new results and
      evaluate runtime memory-order expressions exactly once. The locked
      compare-exchange supplies sequential consistency for all valid orders.
- [x] `test-verified-atomic-fetch-add` covers all six forms on 32-bit values,
      plus 8/16-bit values on i686/AMD64 and 64-bit values on AMD64. Both
      targets pass at `-O0`/`-O2` with zero verified-backend fallback; AMD64
      objects execute and agree with the GCC C17 oracle. Atomic builtin,
      language, and IR/MIR/x86 encoding suites pass.
- [ ] Generic/aggregate builtin forms, floating/aggregate atomics, and general
      C++ atomic operations remain open.

## RCC continuation (2026-10-21, typed-SSA atomic test-and-set/clear)

- [x] Lower `__atomic_test_and_set`, `__atomic_clear`, and
      `__sync_synchronize` directly to typed SSA. Test-and-set uses byte memory
      `XCHG` and returns the prior byte's truth value; clear uses atomic byte
      `XCHG` with zero; synchronize reuses the sequentially consistent local
      stack-word fence. Runtime order expressions are evaluated exactly once.
- [x] `test-verified-atomic-fetch-add` covers consecutive test-and-set results,
      clear's final byte value, order-expression counts, zero fallback on both
      targets at `-O0`/`-O2`, AMD64 execution, and the GCC C17 oracle.
- [ ] Other atomic integer RMW operators, generic/aggregate builtin forms,
      floating/aggregate atomics, and general C++ atomic operations remain open.

## RCC continuation (2026-10-21, typed-SSA atomic fences)

- [x] Lower `__atomic_thread_fence` and `__atomic_signal_fence` directly to a
      locked memory `XCHG` on an initialized, aligned stack word. This supplies
      an x86 sequentially consistent hardware/compiler barrier, stronger than
      every valid fence order, and evaluates the runtime order expression once.
- [x] `test-verified-atomic-fetch-add` covers side-effecting runtime orders,
      zero fallback at `-O0`/`-O2` for i686/AMD64, AMD64 execution, and the GCC
      C17 oracle; existing `test-ir` verifies the x86 XCHG encoding.
- [ ] Other atomic integer RMW operators, generic/aggregate builtin forms,
      floating/aggregate atomics, and general C++ atomic operations remain open.

## RCC continuation (2026-10-21, typed-SSA NAND/min/max atomic RMW)

- [x] Lower `__atomic_fetch_nand`, `__atomic_nand_fetch`,
      `__atomic_fetch_min`, and `__atomic_fetch_max` with typed-SSA
      compare-exchange retry loops. NAND is width-masked; min/max comparisons
      follow the signedness of the atomic object. Runtime order expressions are
      evaluated once and the locked operation is sequentially consistent.
- [x] `test-verified-atomic-fetch-add` covers fetch/new NAND results and
      signed/unsigned min/max state and prior values. Both targets pass at
      `-O0`/`-O2` with zero verified-backend fallback; AMD64 objects execute
      and agree with a GCC C17 oracle.
- [ ] Generic/aggregate builtin forms, floating/aggregate atomics, and general
      C++ atomic operations remain open.

## RCC continuation (2026-10-21, typed-SSA unsigned-wrap and sync bitwise RMW)

- [x] Lower `__atomic_fetch_uinc` and `__atomic_fetch_udec` using bounded
      compare-exchange loops: increment wraps to zero at/above the limit, while
      decrement wraps to the limit at zero or above it. Lower all legacy
      `__sync_{fetch_and_,}{and,or,xor,nand}` old/new forms to the same
      sequentially consistent loop; their arguments are evaluated once.
- [x] `test-verified-atomic-fetch-add` covers uinc/udec boundary cases,
      side-effecting orders, and each legacy sync AND/OR/XOR/NAND old/new form.
      Both targets pass at `-O0`/`-O2` with zero verified-backend fallback;
      AMD64 objects execute and match the GCC C17 oracle (with host
      compare-exchange oracles for the nonstandard uinc/udec builtins). Atomic
      builtin, language, and IR/MIR/x86 suites pass.
- [ ] Generic/aggregate builtin forms, floating/aggregate atomics, and general
      C++ atomic operations remain open.

## RCC continuation (2026-10-21, typed-SSA legacy sync arithmetic RMW)

- [x] Lower `__sync_fetch_and_add`, `__sync_fetch_and_sub`,
      `__sync_add_and_fetch`, and `__sync_sub_and_fetch` directly to typed SSA
      using sequentially consistent `LOCK XADD`. Preserve old/new results and
      evaluate each operand expression once; leave no-order-argument sync calls
      distinct from the explicit-order `__atomic` forms.
- [x] `test-verified-atomic-fetch-add` exercises all four arithmetic and all
      eight bitwise/NAND legacy sync forms plus uinc/udec boundary cases. Both
      targets pass at `-O0`/`-O2` with zero verified-backend fallback; AMD64
      objects execute and match the GCC C17 oracle. Atomic builtin, language,
      and IR/MIR/x86 suites pass.
- [ ] Generic/aggregate builtin forms, floating/aggregate atomics, and general
      C++ atomic operations remain open.

## RCC continuation (2026-10-21, typed-SSA legacy sync lock exchange)

- [x] Lower `__sync_lock_test_and_set`, `__sync_lock_release`, and
      `__sync_swap` directly to typed SSA. Memory `XCHG` provides sequential
      consistency, stronger than the acquire/release requirements of the lock
      operations. Release writes zero, and each builtin argument is evaluated
      once.
- [x] `test-verified-atomic-fetch-add` covers repeated lock acquisition,
      release, and swap. Both targets pass at `-O0`/`-O2` with zero
      verified-backend fallback; AMD64 objects execute and match the GCC C17
      oracle (using `__atomic_exchange_n` as the host oracle for RinOS's
      `__sync_swap`). Atomic builtin, language, and IR/MIR/x86 suites pass.
- [ ] Generic/aggregate builtin forms, floating/aggregate atomics, general
      C++ atomic operations, and legacy sync compare-exchange builtins remain
      open.

## RCC continuation (2026-10-21, typed-SSA legacy sync compare-exchange)

- [x] Lower `__sync_bool_compare_and_swap` and
      `__sync_val_compare_and_swap` directly to typed SSA and locked
      `CMPXCHG`. The bool form compares the observed old value with the expected
      value; the value form returns that observed old value. All arguments are
      evaluated once before the full-barrier operation.
- [x] `test-verified-atomic-fetch-add` covers both forms' success and failure
      paths, result values, final state, and input-expression evaluation counts.
      Both targets pass at `-O0`/`-O2` with zero verified-backend fallback;
      AMD64 objects execute and match the GCC C17 oracle. Atomic builtin,
      language, and IR/MIR/x86 suites pass.
- [ ] Generic/aggregate builtin forms, floating/aggregate atomics, and general
      C++ atomic operations remain open.

## RCC continuation (2026-10-21, typed-SSA generic floating atomics)

- [x] Lower generic `__atomic_load`, `__atomic_store`, `__atomic_exchange`,
      and `__atomic_compare_exchange` for 32-bit `float` on i686/AMD64 and
      64-bit `double` on AMD64. Exchange/CAS operate on same-width integer
      bitcasts so signed zero and NaN payloads are compared by representation;
      atomic stores use integer-width sequentially-consistent `XCHG`.
- [x] Add `test-verified-atomic-fetch-add` coverage for generic float load,
      store, exchange, failed/successful CAS, and +0/-0 representation mismatch;
      AMD64 double coverage exercises all four generic operations. Both targets
      pass `-O0`/`-O2` with zero verified-backend fallback, GCC C17 oracle and
      AMD64 object execution pass, and the runner now resolves local `.rodata`
      `REL32`/`ABS64` references needed by floating constants.
- [x] `test-atomic-builtins`, `test-atomic-language`, and `test-ir` pass.
- [ ] Floating support remains limited to generic builtins and lock-free
      32-bit `float` on both targets / 64-bit `double` on AMD64. Standard
      `stdatomic.h` floating typedef/API coverage, aggregate and wider floating
      objects, and general C++ atomic conformance remain open.

## RCC continuation (2026-10-21, C++ scalar/pointer `std::atomic` and `atomic_flag`)

- [x] Add the compiler's `<atomic>` header with `std::memory_order` and a
      scalar-integer `std::atomic<T>` surface for initialization, load/store,
      conversion, assignment, exchange, weak/strong compare-exchange,
      fetch-add/sub/and/or/xor, and increment/decrement/compound arithmetic.
      Operations delegate to the compiler's checked atomic builtins and use
      their established x86 sequentially-consistent implementations.
- [x] Extend the C++ atomic fixture with default/value initialization,
      lock-free query, 32-bit and 64-bit integer operations, explicit/default
      order overloads, successful and failed compare-exchange, old/new
      results, and pointer `difference_type` arithmetic. The pointer
      specialization scales `fetch_add/sub` by pointee size and covers pointer
      pre/post increment/decrement, compound updates, and CAS.
      Add C++20 `atomic_flag`, `ATOMIC_FLAG_INIT`, the clear initial state, and
      `test_and_set`, `clear`, and `test` with explicit/default order forms.
      `test-atomic-builtins` compiles i686/AMD64 C++20 objects at `-O0`/`-O2`
      and executes both AMD64 objects; all existing C atomic execution and
      invalid order/pointer diagnostics pass.
- [x] Make `test-atomic-builtins` an explicit C++20 regression prerequisite
      and include it in both C17 and C++ regression aggregates.
- [x] Add C++20 floating scalar atomic operations to the shared `atomic<T>`
      API: `float` load/store/conversion/assignment, exchange, weak/strong
      compare-exchange, and CAS-loop `fetch_add/sub` on i686 and AMD64; add the
      same support for `double` on AMD64. Default initialization and arithmetic
      results compile on both targets and execute in AMD64 `-O0`/`-O2` runs via
      `test-atomic-builtins`.
- [x] Add volatile-qualified `atomic_flag` and scalar/pointer/floating
      `atomic<T>` overloads for the operations currently provided by the
      header. Fix implicit-object overload ranking to count each added cv
      qualifier so mutable objects prefer `const` over `const volatile`.
      `test-atomic-builtins` executes integer, pointer, float, AMD64 double,
      and flag operations through volatile objects; the existing volatile
      member-overload tests pass on both target widths.
- [ ] Full `<atomic>` conformance remains open: type constraints/traits,
      complete constructor/API overload sets, and C++20 wait/notify operations.

## RCC continuation (2026-10-23, C++20 atomic bitwise compound assignment)

- [x] Add `std::atomic<T>::operator&=`, `operator|=`, and `operator^=` for
      mutable and volatile objects. Return the post-operation value from the
      existing fetch operations; cover native-width, byte-width, and volatile
      values. Extend C++ compound-assignment rewriting to route member-template
      operators through the existing member-call overload-resolution path,
      instead of misclassifying class operands as built-in integer operations.
      `test-atomic-builtins` compiles i686/AMD64 at `-O0`/`-O2` and executes
      AMD64 outputs; `test-cxx-assignment-operator` verifies existing
      non-template assignment operators and i686/AMD64 code generation.
- [ ] C++ atomic type constraints/traits, remaining API overloads, and
      C++20 wait/notify operations remain open.

## RCC continuation (2026-10-23, C++ atomic non-member operations)

- [x] Add the standard non-member `atomic_is_lock_free`, load/store,
      exchange, weak/strong compare-exchange, and `atomic_flag` test-and-set/
      clear operation families, including explicit memory-order forms. Verify
      mutable, volatile, const-volatile scalar, and pointer objects with the
      C++ atomic regression on i686/AMD64 at `-O0`/`-O2`, native execution, and
      a GCC C++20 standard-library syntax oracle.
- [ ] C++ atomic type constraints/traits, remaining API overloads, and C++20
      wait/notify operations remain open.

## RCC continuation (2026-10-23, legacy atomic initialization compatibility)

- [x] Provide deprecated-source-compatible `ATOMIC_VAR_INIT` and
      `atomic_init` forms for scalar and pointer atomics. Verify initialization
      and relaxed reinitialization through RCC native execution and the GCC
      C++20 standard-library syntax oracle in `test-atomic-builtins`.
- [x] Expose `atomic_thread_fence`, `atomic_signal_fence`, and
      `kill_dependency`; fence functions forward to implemented compiler atomic
      fence builtins and `kill_dependency` preserves its value. Verify scalar
      and pointer use in both RCC target builds, native execution, and the GCC
      C++20 standard-library syntax oracle.
- [x] Define the standard `ATOMIC_*_LOCK_FREE` macros for fundamental and
      pointer types as always-lock-free on the supported i686/AMD64 targets;
      compile-time assertions in `test-atomic-builtins` check all published
      values against GCC's C++20 standard header and both RCC targets.

## RCC continuation (2026-10-22, C++20 i686 `std::atomic<double>`)

- [x] Enable generic 64-bit floating atomic load/store/exchange/compare-exchange
      on i686 through the existing `CMPXCHG8B` lowering; C++ CAS loops then
      provide `double` fetch-add/sub without integer or floating representation
      aliasing. Regressions cover default initialization, lock-free query,
      volatile/nonvolatile load/store/exchange/CAS/fetch-add/fetch-sub and
      assignment. `make test-atomic-builtins` compiles C++20 i686/AMD64 at
      `-O0`/`-O2`, executes both i686 and AMD64 objects with their native host
      runners, and passes the existing invalid-builtin diagnostic checks.
- [x] Expose `std::atomic<T>::value_type` through the shared atomic base and
      verify the dependent nested type on i686/AMD64 C++20 builds plus both
      native atomic execution runners in `test-atomic-builtins`.
- [x] Expose `is_always_lock_free` on `std::atomic<T>` and `atomic_flag`;
      resolve inherited static constexpr data members through both object and
      class-qualified class-template expressions. Constructor lowering now
      skips static fields when checking instance initializer lists. The
      dual-architecture `test-atomic-builtins` execution regression exercises
      these paths at `-O0` and `-O2`.
- [x] Keep constructor-binding temporary cleanup from freeing AST-arena
      expressions. The i686 and AMD64 `test-cxx-dependent-dmi-call-argument`
      executable regression covers dependent default-member-initializer calls.
- [ ] C++20 `<atomic>` constraints/type traits, complete constructor/API
      overload sets, and wait/notify operations remain open.

## RCC continuation (2026-10-22, inherited C++ static data members)

- [x] Resolve inherited static constexpr data-member lookup in class-template
      bases for both object access and `ClassTemplate<T>::member` access, and
      fold integral constexpr initializers without overwriting the value in
      the shared expression union. The C++20 atomic regression executes both
      forms on i686 and AMD64 at `-O0` and compiles them at `-O2`.
- [x] Ignore static members when deciding whether constructor member
      initializers match instance storage; preserve real diagnostics for
      unsupported instance constructors.
- [x] Keep AST-arena `this` expressions out of the temporary malloc cleanup
      list in both code generators. `test-cxx-dependent-dmi-call-argument`
      now generates and executes i686/AMD64 binaries without invalid frees.
- [x] Resolve inherited static-function overloads through object and
      class-qualified class-template expressions without inventing an object
      parameter; the atomic fixture executes both one- and two-argument
      overloads on i686 and AMD64 at `-O0` and compiles at `-O2`. The focused
      invalid fixture rejects class-qualified inherited non-static calls with
      an object-required diagnostic on both targets.
- [ ] The remaining `<atomic>` API conformance work remains open.

## RCC continuation (2026-10-21, C++ `static_cast` conversion functions)

- [x] Resolve implicit and explicit conversion functions when a C++
      `static_cast` converts a class object to an arithmetic or pointer target.
      Explicit conversion functions participate only when their result is an
      exact match for the cast target; ordinary initialization, return, and
      overload conversion continue to reject explicit conversion functions.
- [x] Add a focused regression with side-effecting conversion operators so
      returning the source object's first field cannot pass accidentally;
      verify ranking between exact explicit and standard-conversion implicit
      results and an rvalue source. i686/AMD64 code generation, AMD64 native
      execution, GCC C++20 execution, and i686/AMD64 rejection of implicit use
      of an explicit operator or an extra conversion after an explicit
      operator pass in
      `test-cxx-static-cast-conversion-operator`.
- [x] Apply explicit conversion-function lookup to `static_cast` expressions
      targeting pointer-to-member types. Verify the returned member designator
      is used, both target widths assemble, AMD64 executes against a GCC C++20
      oracle, and implicit return conversion from the explicit operator is
      still rejected in `test-cxx-static-cast-conversion-operator`.
- [ ] Full C++ explicit-cast coverage remains open for pointer-to-member
      adjustment edge cases and less-common standard conversion sequences.

## RCC continuation (2026-10-21, C++ class-target `static_cast`)

- [x] Lower class-target `static_cast` through the C++ direct-initialization
      and constructor overload path, including explicit converting
      constructors; preserve the built-in derived-to-base class-value
      conversion and base-subobject adjustment before that path. Verify the
      selected explicit overload and base slicing by their results, i686/AMD64
      code generation, AMD64 default and `-O2` execution, and a GCC C++20
      oracle in `test-cxx-static-cast-constructor`.
- [x] Reject a scalar-to-empty-class `static_cast` with no viable constructor
      on i686/AMD64, and preserve rejection of copy-initialization through an
      explicit constructor.
- [x] Preserve direct-constructor selection when a source also has an
      explicit conversion operator to the target class; when no direct
      constructor is viable, lower that explicit conversion operator. The
      competing paths produce distinct values and pass the GCC C++20 oracle,
      i686/AMD64 code generation, and AMD64 default/`-O2` execution in
      `test-cxx-static-cast-constructor`.
- [x] In C++17 and later, initialize a same-type nontrivial class local or
      static-storage object directly in its destination from a class-returning
      call, avoiding an extra copy or temporary. Static-cast conversion
      results verify exact local destructor counts, global initialization
      values, a GCC C++20 oracle, i686/AMD64 assembly, and RCC default/`-O2`
      AMD64 execution in `test-cxx-static-cast-constructor`.
- [x] Apply C++20 aggregate parenthesized-initialization rules to
      class-target `static_cast`, including value-initialization of omitted
      members and omitted default member initializers. Verify local and global
      casts against GCC, assemble both target widths, and execute RCC AMD64 at
      default optimization and `-O2` in `test-cxx-static-cast-constructor`.
- [x] Run nontrivial by-value class parameter destructors on both explicit
      return and fallthrough paths. Static-cast prvalues exercise the parameter
      lifetime; verify exact counts against GCC, i686/AMD64 generation, and
      AMD64 default/`-O2` execution in `test-cxx-static-cast-constructor`.
- [x] Pass nontrivial by-value class parameters through the Itanium invisible-
      pointer ABI, directly constructing prvalue arguments as the callee's
      parameter object and copy-initializing a distinct object for lvalue
      arguments. Cover a seventh GP-class argument (stack spill), object
      identity, exact destructor counts, GCC comparison, i686/AMD64 assembly,
      and AMD64 default/`-O2` execution in
      `test-cxx-static-cast-constructor`.
- [x] Preserve nontrivial by-value parameter cleanup when an exception escapes
      through a nested call, and when the callee returns from an internal
      `try`. Search the active exception-frame chain when unregistering cleanup
      records, and make an absent record a no-op when no frame owned it.
      `test-cxx-exception-cleanup` coverage passes AMD64 default/`-O2` runtime,
      i686/AMD64 object emission, and verified backend on both architectures
      with 55 functions and zero fallback. The test command's remaining Windows
      shell-path issue is environmental; its compiler, format, verified, and
      runtime steps were run directly.
- [ ] Remaining class-target cast coverage includes other aggregate
      parenthesized-initialization edge cases, other class-prvalue uses outside
      same-type local/static-storage initialization, and overloaded conversion
      functions with non-exact result sequences.

## RCC continuation (2026-10-21, class-returning `operator->` proxies)

- [x] Follow overloaded `operator->` calls whose intermediate result is a
      class prvalue, continuing until the chain produces a pointer. Preserve
      intermediate proxy full-expression destruction; the nested proxy/member
      call/destructor sequence is verified as `1234` against GCC C++20.
- [x] Verify scalar return rejection and a class return without a subsequent
      `operator->` on i686/AMD64. The test gate also compiles optimized objects
      for both target widths and runs AMD64 at default optimization and `-O2`.
      The Windows make shell could not resolve its configured `objdump` path in
      this environment; all codegen, object-format, diagnostic, oracle, and
      runtime steps were run directly.
- [x] Select `&`, `const&`, and `&&` proxy `operator->` overloads correctly,
      and follow an intermediate class lvalue-reference result without
      inventing a temporary. GCC C++20, i686/AMD64 default/`-O2` object
      generation, and AMD64 default/`-O2` execution pass.
- [x] Keep the single pointer-returning `operator->` form in typed SSA with
      zero fallback for both targets; emit three functions at `-O2`.
- [x] Lower the implicit object for a class-prvalue member call from the
      aggregate result storage. A class-returning proxy with a nontrivial
      destructor emits four functions with zero typed-SSA fallback on i686
      and AMD64 at `-O0` and `-O2`; GCC C++20 and RCC AMD64 execution verify
      exactly-once cleanup.
- [x] Remove the fixed 64-call semantic limit and diagnose a repeated
      `operator->` function/class/cv/value-category state as circular. A
      65-class-return chain passes GCC and RCC x64 execution; i686/AMD64 typed
      SSA emits with zero fallback at `-O0`/`-O2`, the i686 `-O2` object is
      PE-i386, and circular-chain diagnostics pass for both target widths.

## RCC continuation (2026-10-21, parenthesized aggregate reference temporaries)

- [x] Keep a nontrivial class prvalue bound to a reference member during C++20
      aggregate parenthesized initialization alive through the remaining
      element initializers, then destroy it exactly once at the full-expression
      boundary. Track a caller-frame owner and cleanup plan on the initializer
      element and lower it in both legacy target backends. The regression
      observes the temporary from a later element initializer, compares with
      GCC C++20, emits i686/AMD64 objects at default and `-O2`, verifies
      PE-i386, and executes AMD64 at both optimization levels.
- [x] Lower namespace-scope static-duration C++20 parenthesized aggregate
      initialization for trivially-copyable class aggregates, including the
      reference-member temporary cleanup above. Test global initialization
      order and full-expression cleanup against GCC; emit i686/AMD64 objects at
      default and `-O2`, verify PE-i386, and execute AMD64 at both levels.
- [x] Extend guarded initialization to block-scope `static` trivially-copyable
      class aggregates with C++20 parenthesized initializers. Verify one-time
      initialization and repeat access with a different counter, generate
      i686/AMD64 at `-O0`/`-O2`, and execute AMD64 at both levels.
- [x] Preserve optimizer correctness when a side-effecting call appears in a
      reference initializer: keep the lvalue address intact while invalidating
      local scalar constant facts that the call may change indirectly. The
      static aggregate lifetime counter regression passes at `-O2`.
- [x] Do not interpret a reference parameter's union-backed `param_index` as a
      variable initializer during callee-side lifetime analysis. Reference
      temporary lifetime belongs to the call site; the dual-architecture
      `test-optimize` cleanup compilation and execution checks pass.
- [x] Lower this reference-member temporary cleanup and exceptional-unwind
      fixture through verified-SSA on i686 and AMD64 at `-O0` and `-O2`.
      Require all four functions and zero fallback notices for each target and
      optimization level; `test-cxx-aggregate-paren-init-posix` passes.
- [x] Compile the local non-trivially-copyable `NonTrivialAggregate` target,
      including its user-defined destructor and C++20 parenthesized
      initialization, through verified-SSA on i686 and AMD64 at `-O0` and
      `-O2`. Require both functions and zero fallback notices in each
      configuration; the same gate executes the legacy-backend lifetime test
      against GCC's C++20 oracle.
- [x] Return a non-trivially-copyable aggregate produced by C++20
      parenthesized initialization directly from a function. Verify guaranteed
      copy elision through exactly-once destructor counts, GCC C++20, i686 and
      AMD64 output at `-O0`/`-O2`, AMD64 execution at both levels, and all three
      verified-SSA functions with zero fallback on both targets/optimization
      levels. The `test-cxx-aggregate-paren-init` gates pass on POSIX and native
      Windows with isolated output trees.
- [ ] Additional non-trivially-copyable aggregate contexts, thread-local
      initialization, and other class-prvalue contexts remain open.

## RCC continuation (2026-10-21, conversion-function member-pointer adjustment)

- [x] Verify the base-to-derived member-pointer adjustment over a transitive
      two-level non-virtual owner path with nonzero offsets at both levels.
      Exercise both data- and member-function pointers returned by conversion
      functions; GCC C++20, i686/AMD64 legacy output, AMD64 default/`-O2`
      execution, and all thirteen verified-SSA functions with zero fallback at
      both architectures and optimization levels pass in
      `test-cxx-static-cast-conversion-operator`.
- [x] Preserve the standard base-to-derived data- and member-function-pointer
      conversions after class conversion functions return base-owned member
      pointers in `static_cast`. Verify GCC C++20 parity, i686/AMD64 legacy
      code generation, AMD64 execution at default optimization and `-O2`, and
      verified-SSA emission of all thirteen functions with zero fallback on
      i686/AMD64 at `-O0`/`-O2` in
      `test-cxx-static-cast-conversion-operator`.
- [x] Compare non-exact conversion-function result sequences for
      `static_cast<double>`: `float`-to-`double` promotion must beat
      `int`-to-`double` conversion. Check the selected function's runtime
      result and call counts against GCC, i686/AMD64 generation, AMD64
      execution at default/`-O2`, and all thirteen verified-SSA functions with
      zero fallback in the same target.
- [x] Prefer an exact pointer result over one that needs a cv-qualification
      conversion when both trailing sequences have Exact Match rank. For a
      `const int*` target, select `operator const int*()` over `operator int*()`;
      verify both `static_cast` and by-value argument conversion against GCC,
      i686/AMD64 generation, AMD64 default/`-O2` execution, and all sixteen
      verified-SSA functions with zero fallback at both targets/optimization
      levels in `test-cxx-static-cast-conversion-operator`. The focused target
      and aggregate `test-cxx-conversion-operator` pass.
- [ ] Other pointer-to-member adjustment edge cases and standard conversion
      sequence ordering beyond promotion-versus-conversion and
      exact-versus-qualification remain open.

## RCC continuation (2026-10-21, conversion-function member-pointer owner ranking)

- [x] For conversion functions returning member pointers owned by related
      non-virtual bases, prefer the result owned by the nearer base when both
      convert to a member-pointer target owned by the same derived class.
      Verify explicit `static_cast` and implicit function-argument conversion
      against GCC, execute the selected member through i686-generated and
      AMD64 default/`-O2` output, and retain ambiguity for unrelated base
      owners. The focused gate checks i686 PE object format and verifies that
      ambiguous `.ro` output is absent on both target widths.
- [x] Preserve the selected conversion function's declared result type through
      ABI lowering, then represent its required base-to-derived member-pointer
      owner adjustment as a separate cast AST node for implicit by-value
      arguments. The focused gate and `test-cxx-conversion-operator` pass; all
      four functions emit with zero verified-backend fallback for i686/AMD64
      at `-O0` and `-O2`.
- [ ] Other member-type qualification/noexcept combinations,
      virtual/ambiguous owner paths, and broader standard conversion-sequence
      ordering remain open.

## RCC continuation (2026-10-21, dynamic nested automatic-array DWARF)

- [x] Preserve inline pointer arguments formed by subscripting a fixed-size
      automatic two-dimensional array with independently frame-backed row and
      column indexes. Also compose either dimension with a constant subscript.
      Verify the matrix frame slot, both dynamic-index slots, the row stride
      (20 bytes), and column stride (4 bytes) in i686/AMD64 legacy and verified
      optimized DWARF. `test-debug-info` passes.
- [x] Preserve inline pointer arguments formed by subscripting a
      frame-backed pointer-to-fixed-array through independently dynamic row and
      column indexes, or a constant in either dimension. Verify the pointer
      frame slot, both row/column scales (20/4 bytes), and constant byte
      adjustments (20/8 bytes) in i686/AMD64 legacy and verified optimized
      DWARF. Native-Windows `test-debug-info` passes.
- [ ] VLA and pointer-to-pointer/indirect nested subobjects, target-adjusting
      casts, dynamic pointer arithmetic beyond the separately covered bounded
      forms, mutable parameter tracking, and optimized register/piece locations
      remain open.

## RCC continuation (2026-10-21, i686 unsigned 64-bit inline addition DWARF)

- [x] Describe an inline `unsigned long long` argument formed from a stable
      frame-backed `unsigned long long` plus a same-typed integer constant on
      i686. Emit the result as exact low/high four-byte `DW_OP_piece` values,
      including unsigned carry into the high word. The verifier checks the
      frame slots and opcode stream, and exercises wrap/carry boundary values.
      Native-Windows `test-debug-info-inline-expression` and full
      `test-debug-info` pass.
- [ ] General i686 unsigned 64-bit arithmetic, nonconstant operands, and other
      optimized multi-piece locations remain open.

## RCC continuation (2026-10-22, i686 unsigned 64-bit inline bitwise DWARF)

- [x] Preserve inline `unsigned long long` arguments formed by AND, OR, or XOR
      with a same-typed integer constant on i686, and unary `~` from a
      frame-backed source. Emit exact low/high four-byte `DW_OP_piece` values.
      Verify each opcode, constant word, source offset, piece width, and eight
      boundary patterns in legacy and verified-backend i686/AMD64 objects.
      Native-Windows
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Lower frame-backed i686 `unsigned long long` subtraction by a same-typed
      constant into low/high pieces, with an exact unsigned borrow into the
      high word. Verify the DWARF expression and wrap/borrow boundary patterns;
      both inline-expression and full debug-info gates pass.
- [x] Preserve i686 unary bitwise NOT on frame-backed `unsigned long long` as
      two exact four-byte value pieces. Verify source offsets and piece widths
      plus eight boundary values; both debug-info gates pass.
- [x] Preserve i686 `unsigned long long` multiplication by a same-typed integer
      constant as low/high four-byte pieces. Compute the low-word product carry
      with exact 16-bit partial products, then combine both cross-products and
      the carry modulo 64 bits. Verify the emitted opcode stream, source frame
      slot, piece widths, and eight boundary values in legacy and verified
      i686/AMD64 objects; both native-Windows debug-info gates pass.
- [x] Preserve unary negation of a frame-backed i686 `unsigned long long` as
      low/high four-byte pieces, subtracting the low-word nonzero flag as the
      high-word borrow. Verify the opcode stream, source frame slot, piece
      widths, and eight boundary values in legacy and verified i686/AMD64
      objects; both native-Windows debug-info gates pass.
- [x] Preserve i686 unsigned 64-bit addition between two frame-backed locals
      as low/high four-byte pieces. Derive the low-word carry with a
      sign-bit-flipped comparison and add it to the high-word sum. Verify exact
      opcodes, both source frame slots, piece widths, and eight carry/wrap
      patterns in legacy and verified-backend objects; the focused and full
      native-Windows debug-info gates pass.
- [x] Preserve i686 unsigned 64-bit subtraction between two frame-backed
      locals as low/high four-byte pieces. Derive the low-word borrow with a
      sign-bit-flipped unsigned comparison and subtract it from the high-word
      difference. Verify exact opcodes, both source frame slots, piece widths,
      and eight borrow/wrap patterns in legacy and verified-backend objects;
      the focused and full native-Windows debug-info gates pass.
- [x] Preserve frame-backed i686 unsigned 64-bit AND, OR, and XOR between two
      locals as separate low/high four-byte pieces. Verify the exact operation
      per word, both source frame locations, piece widths, and eight
      reconstructed boundary values in legacy and verified-backend objects;
      the focused and full native-Windows debug-info gates pass.
- [x] Preserve frame-backed i686 unsigned 64-bit multiplication between two
      locals. Emit the low 32-bit product, derive the exact high word of the
      low-word product from 16-bit partial products (including middle carry),
      then add both cross-products. A test-side DWARF evaluator runs eight
      high-bit and wraparound operand pairs against the emitted two-piece
      expression in legacy and verified-backend objects; both debug-info gates
      pass.
- [x] Preserve i686 unsigned 64-bit runtime division/remainder when both
      operands are non-TLS function-local statics. Emit source-file-scoped
      relocations for each storage symbol and verify initialized high-bit
      operands plus the 11-by-9 division/remainder boundary matrix in normal
      and verified C++20 objects. Focused and full native-Windows
      `test-debug-info` gates pass.
- [ ] TLS or volatile/reference/atomic operands, wider computed i686 64-bit
      division/remainder expressions, and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-27, pointer arguments in dependent constexpr calls)

- [x] Allow the bounded dependent constexpr member-call evaluator to rank and
      evaluate by-value non-member pointer and `nullptr` arguments. Regressions
      cover typed null, direct `nullptr`, and non-null addresses of a global
      object and a global array element in dependent non-type template
      arguments. Also test a constexpr pointer dereference of that array
      element, a dependent-call dereference of a global scalar, and pointer
      truth in conditional, `&&`, and `||` expressions.
      GCC C++20 and the i686/AMD64 `test-cxx-function-template-overloads`
      generation/execution gate pass.
- [x] Infer the bound of a global incomplete array from its initializer while
      evaluating a dependent constexpr call that dereferences a pointer to an
      array element. The i686/AMD64 generation and AMD64 runtime checks pass,
      along with the GCC C++20 host oracle and focused overload gate.
- [x] Preserve a non-null object-pointer value passed to a constexpr function's
      `const` pointer lvalue-reference parameter during dependent-call
      evaluation. GCC C++20, i686/AMD64 generation, AMD64 execution, and the
      focused overload gate pass.
- [x] Evaluate pointer addition inside a dependent constexpr call, then
      dereference the resulting pointer into a global incomplete array. Recover
      the pointee type from the bound pointer expression where early template
      evaluation has not assigned the expression type. GCC C++20, i686/AMD64
      generation, AMD64 execution, and the focused overload gate pass.
- [x] Evaluate an `int*` argument converted to a `const void*` parameter in a
      dependent constexpr call. GCC C++20 and the focused i686/AMD64 generation
      and AMD64 execution gate pass.
- [x] Evaluate a derived-object address converted to a base-class pointer
      parameter in a dependent constexpr call. GCC C++20 and the focused
      i686/AMD64 generation and AMD64 execution gate pass.
- [x] Convert `nullptr` to a data-member-pointer parameter and evaluate its
      comparison with `nullptr` inside a dependent constexpr call. GCC C++20
      and the focused i686/AMD64 generation and AMD64 execution gate pass.
- [x] Preserve non-null data-member-pointer constants through dependent
      constexpr calls, scalar parameters/local copies, equality with `nullptr`,
      application with `.*` to a by-value constexpr aggregate, and same-owner
      qualification conversion from `T Owner::*` to `const T Owner::*`. Verify
      dependent non-type template argument selection against GCC C++20 and the
      i686/AMD64 generation plus execution gate.
- [ ] Member-function-pointer owner/signature conversions, virtual constexpr
      dispatch outside a unique nonvirtual owner path, data-member-pointer
      owner-conversion paths beyond four inheritance steps, cv conversions
      from const-qualified source member pointers, and broader pointer
      conversions remain open.

## RCC continuation (2026-10-27, constexpr data-member-pointer owner conversion)

- [x] Convert a non-null `T Base::*` value to `T Derived::*` in the bounded
      constexpr evaluator when the inherited member is uniquely reachable via
      an accessible nonvirtual base path and pointee qualification is
      preserved. Apply the converted pointer to a by-value derived aggregate
      argument containing the base subobject during dependent non-type
      template argument selection. Test both a zero-offset single base and a
      nonzero-offset `Base` as the second of two direct bases.
- [x] Preserve static code generation for the fixture's brace-elided
      single-element aggregate base subobject. Verify the fixture against GCC
      C++20, generate and execute both i686 and AMD64 output, pass the focused
      function-template overload gate, and pass the full POSIX `test-cxx`
      aggregate.
- [x] Cover combined owner adjustment and qualification addition from
      `int Base::*` to `const int Derived::*`, with `Base` as the second
      nonzero-offset direct base. A dependent constexpr call applies the
      converted pointer to a by-value derived aggregate. GCC C++20, i686/AMD64
      generation, AMD64 execution, the focused overload gate, and the full
      POSIX `test-cxx` aggregate pass.
- [x] Cover the same `int Base::*` to `const int Derived::*` conversion through
      two accessible nonvirtual inheritance steps, with `Base` nested inside
      the intermediate base and a nonzero accumulated offset. Apply the
      converted pointer to the nested by-value aggregate in a dependent
      constexpr call. GCC C++20, i686/AMD64 generation, AMD64 execution, the
      focused overload gate, and the full POSIX `test-cxx` aggregate pass.
- [x] Cover `int Base::*` to `const int Derived::*` through three accessible
      nonvirtual inheritance steps, with a nonzero accumulated offset. Apply
      the converted pointer to the nested by-value aggregate as a dependent
      constexpr argument. GCC C++20 host execution, i686/AMD64 generation,
      AMD64 execution, and the focused function-template overload gate pass.
- [x] Extend the same conversion through four accessible nonvirtual
      inheritance steps and a nonzero accumulated offset. Apply the pointer to
      the four-level nested by-value aggregate in dependent constexpr
      selection. The exact fixture passes GCC C++20 host execution, i686/AMD64
      generation, and AMD64 execution.
- [x] At the same four-step owner depth, preserve conversions from
      `int Base::*` to `volatile int Derived::*` and
      `const volatile int Derived::*` in dependent constexpr selection; also
      add `const` from a `volatile int Base::*` source. The exact fixture
      passes GCC C++20 host execution, i686/AMD64 generation, and AMD64
      execution.
- [ ] Owner-conversion paths beyond four inheritance steps, further cv
      combinations from qualified source member pointers, and broader pointer
      conversions remain open.

## RCC continuation (2026-10-27, bounded constexpr member-function-pointer values)

- [x] Preserve a non-null pointer-to-member-function designator with an exact
      owner and signature through a dependent constexpr call, scalar parameter,
      local copy, and comparison with `nullptr`. Use the result to select a
      dependent non-type template argument; also verify the null case. The GCC
      C++20 oracle, focused overload gate (including i686/AMD64 generation),
      and full POSIX `test-cxx` aggregate pass.
- [x] Preserve a global constexpr member-function-pointer designator through
      dependent constexpr calls and emit exact-owner nonvirtual global and
      thread-local initializers with a function-symbol relocation and zero
      `this` adjustment. Execute calls through both objects. GCC C++20, focused
      i686/AMD64 generation, AMD64 execution, inspection of i686 code and TLS
      relocations, and the full POSIX `test-cxx` aggregate pass.
- [x] Invoke an exact-owner nonvirtual member-function pointer through `.*`
      from a constexpr function using a by-value trivial aggregate receiver,
      and through `->*` using pointers to constexpr global and nested
      constexpr-call local objects. Preserve the local receiver bytes while
      forwarding its pointer across evaluator frames. Verify direct and
      global-constexpr PMF values, member access through the bound `this`
      object, GCC C++20, focused i686/AMD64 generation and AMD64 execution,
      and the full POSIX `test-cxx` aggregate.
- [x] Convert a nonvirtual `Base::*` to `Derived::*` when the owner crosses a
      unique accessible nonvirtual-base path four levels deep. Verify dependent
      constexpr parameter conversion/local copy and direct invocation through
      a by-value aggregate, including extraction of the correct `Base` subobject
      for `this`. GCC C++20, the focused i686/AMD64 overload gate, and the full
      POSIX `test-cxx` aggregate pass.
- [ ] Other member-function-pointer signature/cv conversions and virtual
      constexpr dispatch beyond the tested global-object/unique-nonvirtual-base
      cases remain open.

## RCC continuation (2026-10-27, inherited member-function-pointer static adjustment)

- [x] Preserve the nonzero `this` adjustment when a nonvirtual method owned by
      a second nonvirtual base is converted to a derived-class member-function
      pointer in global and thread-local initializers. Validate i686/AMD64
      emitted adjustment words, execute both AMD64 calls, compare with a strict
      GCC C++20 host run, and pass the full POSIX `test-cxx` aggregate.
- [ ] Runtime/dynamic virtual constexpr dispatch, virtual-base owner paths,
      secondary-base constexpr object-state access, and further
      member-function-pointer owner/signature conversions remain open.

## RCC continuation (2026-10-27, virtual member-function-pointer static initialization)

- [x] Emit exact-owner virtual member-function pointers in global and
      thread-local initializers with the ABI virtual-slot discriminator and
      zero `this` adjustment; execute calls through a derived override. Also
      apply global/TLS nonvirtual member-function pointers owned by a virtual
      base to a virtually-derived object, exercising virtual-base receiver
      adjustment. Check i686/AMD64 data words, a strict GCC C++20 host oracle,
      AMD64 execution, and the full POSIX `test-cxx` aggregate.
- [ ] Virtual constexpr dispatch beyond the tested global-object/unique-
      nonvirtual-base case and further member-function-pointer owner/signature
      conversions remain open.

## RCC continuation (2026-10-27, bounded constexpr virtual member-function-pointer dispatch)

- [x] Evaluate virtual member-function-pointer calls through a `const Base*`
      when the constexpr receiver is a global or local constexpr `Derived`
      object, or a by-value constexpr-function parameter of that type, and the
      owner is reached through a unique accessible nonvirtual base path.
      Cover both the primary table and a secondary polymorphic base at a
      nonzero offset; apply the pointer conversion adjustment and select the
      derived final overrider from the secondary table slot. Initialize
      explicitly listed scalar base/member subobjects from constexpr
      constructors, including by-value aggregate constructor parameters, and
      read inherited `Base::value` in the final overrider.
      Dependent constexpr-call frames and static assertions distinguish it
      from the base implementation. The strict GCC C++20 oracle, focused
      `test-cxx-function-template-overloads` gate, and full POSIX `test-cxx`
      aggregate pass on the final compiler source.
- [ ] Extend constexpr virtual dispatch to runtime/dynamic object flows,
      virtual-base owner paths, and additional ambiguous or repeated-base
      cases. Broader constructor evaluation (non-empty constexpr constructor
      bodies, omitted/default-initialized subobjects, and other parameter
      forms such as references) and member-function-pointer owner/signature
      conversions remain open.

## RCC continuation (2026-10-27, secondary-base constexpr constructor state)

- [x] Materialize explicitly initialized scalar and by-value aggregate
      parameters plus direct nonvirtual base/member subobjects for empty
      constexpr constructors used by a polymorphic derived object. Verify that
      a virtual final overrider reads the initialized `value` from its
      secondary base and returns the expected result. The focused overload and
      full POSIX `test-cxx` gates pass on the final compiler source.
- [x] Evaluate a non-empty constexpr constructor body after its explicit base
      and member initializers, including `this->field += parameter` against the
      constructed object's target-layout bytes. Verify inherited base state,
      the updated member, strict GCC C++20, dual-architecture focused generation
      and execution, and the full POSIX `test-cxx` aggregate.
- [x] Evaluate omitted direct nonvirtual-base and class-member default
      constructors, together with an omitted scalar member's default-member
      initializer, during constexpr construction. Verify all subobject values,
      strict GCC C++20, and focused i686/AMD64 generation and execution.
- [x] Bind read-only scalar constructor references during constexpr
      materialization: `const int&` from global and constexpr-function-local
      lvalues and a temporary, plus `const int* const&` from a constexpr
      pointer lvalue and a temporary pointer value. Copy the pointer into a
      derived object's member without claiming reference identity or mutable
      aliasing. Verify strict GCC C++20, dual-architecture focused generation
      and execution, and the full POSIX `test-cxx` aggregate.
- [x] Read a scalar pointee through a by-value object-pointer constructor
      parameter in a member initializer. Cover both a global constexpr pointer
      lvalue and a temporary address, with GCC C++20, dual-architecture focused
      generation/execution, and the full POSIX `test-cxx` aggregate.
- [x] Execute a parameter-dependent constructor-body `if/else` that updates
      `this->value` in each branch. Verify positive and negative inputs against
      GCC C++20 and focused i686/AMD64 generation and execution; broader body
      expressions and side effects remain open.
- [x] Honor an empty `return;` from a selected constexpr-constructor-body
      branch. Verify that the early-return branch preserves its mutation and
      that the fallthrough branch still executes, with GCC C++20 and the
      focused i686/AMD64 generation and execution target.
- [x] Preserve a by-value pointer constructor argument's alias to a local
      aggregate inside a constexpr function. Mutate a member through that
      pointer in the constructor body, then verify the caller's aggregate and
      the retained pointer see the updated value with GCC C++20 and the focused
      i686/AMD64 generation/execution target.
- [x] Lower constructors that leave non-volatile scalar members default-
      initialized, including members before and after an explicit initializer.
      Route no-base constexpr constructors through object materialization so
      fully initialized objects evaluate correctly and direct indeterminate
      scalar members are rejected with a targeted diagnostic. Verify the
      invalid diagnostic on both targets and execute the focused i686/AMD64
      runtime regression after assigning each indeterminate member.
- [x] Preserve mutable scalar lvalue-reference aliases into caller locals and
      aggregate fields during constexpr constructor evaluation, including
      forwarding the alias through a nested constructor. Verify that member
      initialization observes the pre-body value and constructor-body writes
      update the caller on constexpr and runtime paths, with strict GCC C++20,
      focused i686/AMD64 generation and execution, and the full POSIX
      `test-cxx` aggregate.
- [x] Bind mutable scalar lvalue-reference parameters in scalar-returning
      constexpr functions to caller locals and aggregate fields, and forward
      the alias across nested constexpr function calls. Verify compile-time
      results and runtime writes with strict GCC C++20, focused i686/AMD64
      generation and execution, and the full POSIX `test-cxx` aggregate.
- [x] Preserve mutable scalar lvalue-reference aliases in aggregate-returning
      constexpr functions. Verify both the returned aggregate contents and
      the caller's post-call scalar value with strict GCC C++20, focused
      i686/AMD64 generation and execution, and the full POSIX `test-cxx`
      aggregate.
- [x] Rebind a caller-local pointer through `int*&` in a scalar-returning
      constexpr function, then dereference the updated pointer to a caller
      aggregate member. Verify constexpr and runtime results, strict GCC C++20,
      and focused i686/AMD64 generation and execution.
- [x] Rebind `Pair*&` and `const int*&` parameters in constexpr functions,
      preserving pointer provenance across assignment and allowing aggregate-
      member/scalar dereference after rebinding. Verify constexpr and runtime
      results, strict GCC C++20, and focused i686/AMD64 generation and execution.
- [x] Rebind an `int**&` parameter to a caller-local pointer object and mutate
      the selected scalar through `**target` inside the nested constexpr call.
      Also evaluate direct local `**` dereference; verify constexpr and runtime
      results, strict GCC C++20, focused i686/AMD64 generation/execution, and
      the full POSIX `test-cxx` aggregate.
- [x] Preserve mutable lvalue-reference aliases to struct objects through
      constexpr constructors, scalar-returning functions, and aggregate-
      returning functions, including nested forwarding, member mutation, and
      whole-object assignment through reference parameters. Verify compile-time
      values and caller-visible mutations against strict GCC C++20 and execute
      the focused i686/AMD64 regressions.
- [x] Preserve mutable lvalue-reference aliases to fixed-size arrays through
      constexpr constructors, scalar-returning functions, and aggregate-
      returning functions. Mutate indexed elements and verify compile-time and
      runtime caller-visible results against strict GCC C++20 and focused
      i686/AMD64 generation and execution.
- [x] Track active members for scalar alternatives in constexpr union objects
      passed by mutable reference. Preserve the active alternative through
      constructor/function aliases, aggregate return and whole-union copy,
      allow a valid scalar-member switch, and reject reading an inactive member
      on both targets with diagnostics matching strict GCC C++20.
- [x] Track active non-scalar aggregate alternatives in constexpr unions passed
      by mutable reference. Verify nested member writes, `Pair -> scalar ->
      Pair` implicit-lifetime transitions, and rejection of an inactive
      aggregate read against GCC C++20; run focused i686/AMD64 generation and
      execution.
- [x] Lower empty constexpr constructors with omitted trivial nested aggregate,
      scalar-array, and direct nonvirtual-base subobjects without writing their
      default-initialized storage. Reject constexpr objects that leave scalar
      state indeterminate through each subobject shape with a targeted
      diagnostic; verify exact diagnostic counts and GCC agreement on i686 and
      AMD64, plus runtime construction on both targets.
- [x] Extend indeterminate-scalar analysis to the bounded straight-line
      constexpr-constructor body profile consisting only of direct
      `this->field = RHS` assignments whose RHS does not read the current
      object. Diagnose an omitted scalar not written by that body, and
      materialize/evaluate an omitted scalar that the body initializes.
      Preserve the unsupported-expression path for omitted nested aggregates.
      Verify GCC C++20, exact diagnostic counts, i686/AMD64 generation and
      runtime behavior, and the full POSIX `test-cxx` suite.
- [x] Permit those direct scalar assignments to read another member only when
      that member is initialized by a constructor mem-initializer or a default
      member initializer. Keep reads of omitted/uninitialized members rejected.
      Verify GCC C++20, the negative diagnostic path, positive constexpr and
      runtime cases on i686/AMD64, and the focused plus full POSIX C++ suites.
- [x] Track preceding direct scalar assignments in constructor-body order so a
      later assignment can read an earlier body-initialized member, while a
      read before the first write remains invalid. Verify a valid two-write
      constexpr/runtime case and an invalid read-before-write case on both
      targets, with GCC C++20 and the full POSIX `test-cxx` suite.
- [x] Analyze bounded `if`/`else` branches in the constructor-body scalar
      assignment profile. Evaluate conditions from bound constexpr constructor
      parameters when possible; otherwise join branch state by definite
      initialization. Accept only the selected initialized path (or members
      initialized on every unresolved path), and reject a constexpr object
      whose selected path leaves a scalar omitted. Verify both-branches,
      selected-branch, and selected-missing-branch cases on i686/AMD64 with GCC
      C++20 and the full POSIX `test-cxx` suite.
- [x] Evaluate `if constexpr` branches in constructor-body initialization
      analysis using the compile-time-selected branch only. Verify that the
      selected branch initializes an omitted scalar and the discarded branch
      is not required, with GCC C++17/C++20 and i686/AMD64 constexpr/runtime
      coverage.
- [x] Permit pre/post increment and decrement of already initialized scalar
      members inside the supported constructor-body assignment RHS, while
      keeping uninitialized reads invalid. Verify constexpr/runtime values,
      GCC C++20, both target architectures, and the full POSIX `test-cxx` suite.
- [x] Permit compound assignments to already initialized scalar members in
      the supported constructor-body profile, without treating a read-modify-
      write of an uninitialized member as initialization. Verify all supported
      scalar compound operators, constexpr/runtime values, GCC C++20, both
      target architectures, and the full POSIX `test-cxx` suite.
- [x] Permit pre/post increment and decrement statements on already
      initialized scalar members in the supported constructor-body profile,
      while rejecting mutation of an uninitialized member. Verify constexpr/
      runtime results, GCC C++20, both target architectures, and the full
      POSIX `test-cxx` suite.
- [x] Respect compile-time-selected conditional and short-circuit RHS paths in
      constructor-body initialization analysis, so an unselected read of an
      omitted member does not reject a valid constexpr constructor while an
      evaluated read remains invalid. Verify GCC C++20, constexpr/runtime,
      i686/AMD64, and the full POSIX `test-cxx` suite.
- [x] Track initialized integer local declarations in constexpr constructor
      bodies, including lexical block scope, so later scalar member assignments
      can use them without accepting reads from uninitialized locals or
      members. Verify GCC C++20, constexpr/runtime, i686/AMD64, and the full
      POSIX `test-cxx` suite.
- [x] Track straight-line writes and increments to initialized integer locals
      in constexpr constructor bodies, updating local values for later member
      initializers without allowing analysis to mutate constructor parameters
      or merging unknown branch-local writes. Verify GCC C++20,
      constexpr/runtime, i686/AMD64, and full POSIX `test-cxx`.
- [x] Permit a default-initialized integer local in the supported constructor
      body only when a straight-line simple assignment initializes it before
      any read. Track this state in constexpr evaluation and initialization
      analysis; reject read-before-write, compound assignment, and increment
      while indeterminate. Verify GCC C++20, constexpr/runtime, i686/AMD64, and
      full POSIX `test-cxx`.
- [x] Track initialized local writes inside constexpr-selected `if` and
      `if constexpr` constructor branches, carrying the selected local value
      forward while keeping unresolved branch-local mutations out of joins.
      Verify both selected paths, GCC C++20, constexpr/runtime, i686/AMD64, and
      full POSIX `test-cxx`.
- [x] Analyze compile-time-bounded `for` loops in constexpr constructor bodies,
      including loop-local initialization, body updates, the increment
      expression, zero iterations, and post-loop member initialization. Verify
      GCC C++20, constexpr/runtime, i686/AMD64, and full POSIX `test-cxx`.
- [x] Analyze compile-time-bounded `while` and `do-while` loops in constexpr
      constructor bodies, preserving their distinct zero-iteration/at-least-
      once behavior and carrying loop-local values into post-loop member
      initialization. Verify GCC C++20, constexpr/runtime, i686/AMD64, and full
      POSIX `test-cxx`.
- [x] Honor constexpr-selected `break` and `continue` exits in bounded
      constructor loops, including the `for` increment after `continue`, while
      stopping at `break` and preserving definitely initialized member state.
      Verify each loop form against GCC C++20 and full POSIX `test-cxx`.
- [x] Track selected bare `return;` exits in constexpr constructor bodies so
      unreachable later writes do not initialize earlier return paths. Verify
      initialized and indeterminate return paths against GCC C++20 and full
      POSIX `test-cxx`.
- [x] Analyze constexpr-selected `switch` cases in constructor bodies,
      including fallthrough, switch-local `break`, default selection, and
      definite initialization of omitted scalar members. Verify matched,
      unmatched, and fallthrough paths against GCC C++20 and full POSIX
      `test-cxx`.
- [x] Allow side-effect-contained calls to free constexpr functions with
      by-value scalar arguments in direct constructor-body member assignments.
      Check arguments for indeterminate member reads; retain conservative
      handling of reference/pointer and member-function calls. Verify GCC C++20,
      constexpr/runtime, i686/AMD64, and full POSIX `test-cxx`.
- [x] Permit those by-value scalar constexpr calls in initialized integer-local
      declarations inside constructor bodies, then use the local for a member
      assignment. Verify GCC C++20, constexpr/runtime, i686/AMD64, and full
      POSIX `test-cxx`.
- [x] Track floating-point constructor-body local initializers and updates,
      preserving the floating representation through local compound assignment
      and subsequent member writes; also clear indeterminate state on a simple
      assignment before a later read and reject read-before-write. Verify GCC
      C++20, constexpr/runtime, i686/AMD64, and full POSIX `test-cxx`.
- [x] Track initialized scalar constructor-body locals addressed by a direct
      pointer local, allowing safe dereference in later member initializers.
      Reject indeterminate pointees and unsupported aggregate/member targets;
      verify integer and floating pointees against GCC C++20, constexpr/runtime,
      i686/AMD64, and full POSIX `test-cxx`.
- [ ] Constructor bodies outside the recognized straight-line assignments,
      omitted nested subobject state when a body is present, mutable references
      to non-trivial union alternatives or untested lifetime transitions,
      other pointer-reference forms not covered by the scalar- and
      aggregate-returning evaluators, indeterminate-state analysis for other
      subobject forms, and complete virtual-base constructor evaluation remain
      open.

## RCC continuation (2026-10-22, i686 signed 64-bit variable inline DWARF)

- [x] Preserve frame-backed i686 signed `long long` addition, subtraction,
      AND, OR, XOR, and multiplication between two variables as exact low/high
      four-byte `DW_OP_piece` values. The regression checks both source frame
      locations and evaluates emitted expressions over signed-safe boundary
      operands in legacy and verified-backend objects. Focused and full
      native-Windows debug-info test gates pass.
- [ ] Nonconstant i686 64-bit division/remainder and broader optimized
      multi-piece locations remain open.

## RCC continuation (2026-10-22, i686 signed 64-bit constant bitwise DWARF)

- [x] Preserve a frame-backed i686 signed `long long` AND, OR, or XOR with a
      same-typed constant as exact low/high four-byte `DW_OP_piece` values.
      Verify signed type metadata, frame location, per-word opcodes, negative
      masks, and eight signed boundary patterns in legacy and verified-backend
      objects. The focused inline-expression and full native-Windows
      `test-debug-info` gates pass.
- [ ] Nonconstant i686 64-bit division/remainder and broader optimized
      multi-piece locations remain open.

## RCC continuation (2026-10-22, i686 signed 64-bit constant inline DWARF)

- [x] Preserve a frame-backed i686 signed `long long` plus or minus a typed
      constant as exact low/high four-byte `DW_OP_piece` values. Verify the
      original frame location, opcode stream, piece widths, and eight
      signed-safe boundary patterns for negative addition and positive
      subtraction in legacy and verified-backend objects. The focused
      `test-debug-info-inline-expression` and full native-Windows
      `test-debug-info` gates pass.
- [ ] Nonconstant i686 64-bit division/remainder and broader optimized
      multi-piece locations remain open.

## RCC continuation (2026-10-22, i686 signed 64-bit unary inline DWARF)

- [x] Preserve frame-backed i686 signed `long long` bitwise NOT and unary
      negation as exact low/high four-byte `DW_OP_piece` values. Verify the
      source frame location, opcode sequence, and eight signed boundary bit
      patterns; exclude `LLONG_MIN` from negation inputs because its signed
      negation is undefined. Focused inline-expression and full native-Windows
      `test-debug-info` gates pass for legacy and verified-backend output.
- [ ] Nonconstant i686 64-bit division/remainder and broader optimized
      multi-piece locations remain open.

## RCC continuation (2026-10-22, i686 signed 64-bit constant multiplication DWARF)

- [x] Preserve a frame-backed signed `long long` multiplied by a same-typed
      constant as exact low/high four-byte `DW_OP_piece` values. Reuse the
      16-bit partial-product high-word calculation, test a negative constant,
      and evaluate eight signed inputs whose products remain representable.
      Focused inline-expression and full native-Windows `test-debug-info`
      gates pass for legacy and verified-backend objects.
- [ ] Nonconstant i686 64-bit division/remainder and broader optimized
      multi-piece locations remain open.

## RCC continuation (2026-10-22, i686 unsigned 64-bit constant shift DWARF)

- [x] Preserve frame-backed i686 unsigned `long long` left/right shifts by a
      constant count from 0 through 63 as exact low/high four-byte
      `DW_OP_piece` values. Regressions verify the cross-word `<< 1` path and
      upper-word `>> 33` path against eight boundary bit patterns in legacy
      and verified-backend DWARF. Focused inline-expression and full
      native-Windows `test-debug-info` gates pass.
- [ ] Nonconstant i686 64-bit division/remainder and broader
      optimized multi-piece locations remain open.

## RCC continuation (2026-10-22, i686 signed 64-bit constant shift DWARF)

- [x] Preserve frame-backed signed `long long` left shifts and arithmetic
      right shifts by constant counts from 0 through 63 as low/high four-byte
      `DW_OP_piece` values. Regressions verify a representable `<< 1` and
      negative/positive `>> 33` values, including exact `DW_OP_shra` sign
      propagation, in legacy and verified-backend DWARF. Focused and full
      native-Windows `test-debug-info` gates pass.
- [ ] Nonconstant i686 64-bit division/remainder and broader
      optimized multi-piece locations remain open.

## RCC continuation (2026-10-22, i686 64-bit variable shift DWARF)

- [x] Preserve frame-backed i686 signed and unsigned `long long` left/right
      shifts with a frame-backed 32-bit runtime count. Emit separate DWARF
      branches for count zero, counts 1–31, and counts 32–63, composing exact
      low/high four-byte pieces; signed right shifts use arithmetic
      sign-fill. The test evaluator executes emitted branch expressions for
      counts 0, 1, 31, 32, 33, and 63 across unsigned boundary patterns and
      signed negative/positive values. Signed-left cases with undefined
      overflow are excluded. Focused inline-expression and full
      native-Windows `test-debug-info` gates pass for legacy and verified
      backends.
- [ ] Other count types/locations, nonconstant i686 64-bit division/remainder,
      and broader optimized multi-piece locations remain open.

## RCC continuation (2026-10-22, i686 64-bit wide shift-count DWARF)

- [x] Extend frame-backed i686 signed and unsigned 64-bit variable shifts to
      frame-backed 64-bit integer count identifiers. For defined C shift
      counts (0–63), the low count word is the full value. Regressions cover
      signed and unsigned left/right operations with both 32-bit and 64-bit
      count variables, evaluating counts 0, 1, 31, 32, 33, and 63 against the
      emitted DWARF expression in legacy and verified-backend objects. The
      focused and full native-Windows `test-debug-info` gates pass.
- [x] Extend i686 signed and unsigned 64-bit variable-shift locations to
      direct global source and global count identifiers. Relocate each
      low/high source word and 32-/64-bit count load independently with
      `ABS32U`, preserving count-zero, 1–31, and 32–63 paths. The evaluator
      exercises counts 0, 1, 4, 31, 32, 33, and 63 for unsigned left/right
      and signed arithmetic-right shifts in legacy and verified-backend
      objects; focused and full native-Windows `test-debug-info` gates pass.
- [ ] Other non-frame-backed count forms/conversions, nonconstant i686 64-bit
      division/remainder, and broader optimized multi-piece locations remain
      open.

## RCC continuation (2026-10-22, dynamic inline floating parameters)

- [x] Preserve direct same-type `float` and `double` identifiers as
      frame-backed locations for inlined parameters. Regressions verify the
      call-specific parameter DIE points to the exact source frame offset in
      legacy and verified i686/AMD64 optimized objects; full native-Windows
      `test-debug-info` passes.
- [ ] Dynamic floating expressions/conversions beyond same-type
      frame-backed identifiers, `long double`, register-only locations, and
      general optimized inline-variable tracking remain open.

## RCC continuation (2026-10-22, i686 unsigned 64-bit constant divmod DWARF)

- [x] Preserve frame-backed i686 `unsigned long long` division by constants
      1 through 2^30 and remainder by the same range as exact low/high
      four-byte `DW_OP_piece` values. Stream base-2^1 through base-2^16
      long-division digits on the DWARF stack, selecting the largest radix
      that keeps every partial dividend in positive signed 32-bit range;
      assemble output pieces by bit position without duplicating prior
      remainder expressions. Regressions semantically evaluate ten 64-bit
      boundary values for `/32769`, `%65536`, `/65537`, `%131072`, and
      `/1073741823` and `%1073741823` in legacy and verified-backend i686
      objects, including the widest supported radix-2 digit stream. The
      focused native-Windows gate and GCC ASan/UBSan/LeakSanitizer
      `test-debug-info` gate pass; the focused gate also passes on Linux.
- [ ] Variable divisors, signed 64-bit division/remainder, non-frame-backed
      operands, and broader optimized multi-piece locations remain open.
- [x] Include the full `test-debug-info` corpus in `test-sanitize` and resolve
      the compiler-lifetime leaks it exposed: release label/function-resolution
      lists after patching on both backends, and allocate semantic temporary
      symbol/template-parameter names in the AST arena that owns their
      declarations. The complete GCC ASan/UBSan/LeakSanitizer
      `test-debug-info` target passed on Ubuntu-24.04 with no leak report.

## RCC continuation (2026-10-22, i686 computed inline shift counts)

- [x] Emit frame-backed i686 DWARF values for a bounded side-effect-free
      integer expression used as the dynamic count of a 64-bit inline shift.
      32-bit count expressions use the bounded scalar DWARF builder; computed
      64-bit counts support frame-backed identifiers, literals, unary negation
      and complement, plus/subtract/multiply, and bitwise operations by
      emitting their exact low word. Variable inputs require stable frame
      locations. `count + 1u` and `count + 1ull` regressions semantically
      evaluate shifts 0, 1, 31, 32, 33, and 63, including defined unsigned
      wrap from `UINT32_MAX`/`UINT64_MAX` to zero. Direct 32-bit and 64-bit
      count cases remain covered. Focused and full `test-debug-info`, plus
      ASan/UBSan `test-debug-info`, pass on Linux for legacy and verified
      output.
- [ ] Non-frame-backed count operands, 64-bit count expressions outside the
      low-word-preserving subset above, variable i686 64-bit
      division/remainder, and broader optimized multi-piece locations remain
      open.

## RCC continuation (2026-10-22, i686 wide shift-count power-of-two remainder)

- [x] Preserve a frame-backed 64-bit shift-count expression reduced modulo a
      power-of-two constant through 2^32 by masking its exact low word. The
      `count % 64ull` regression uses nonzero high-word inputs and evaluates
      defined shift counts 0, 1, 31, 32, 33, and 63 from emitted DWARF in both
      legacy and verified i686 objects; the focused
      `test-debug-info-inline-expression` gate passes. This applies only to
      low-word-preserving remainders; it does not implement general 64-bit
      division/remainder in DWARF.
- [ ] General 64-bit computed count division/remainder, non-frame-backed
      operands, and broader optimized multi-piece locations remain open.

## RCC continuation (2026-10-22, i686 signed wide shift-count remainder)

- [x] Preserve a signed frame-backed i686 `long long` shift-count expression
      reduced by a power-of-two remainder after optimizer strength reduction.
      Recognize the exact sign-corrected quotient/remainder tree and lower its
      low word to the corresponding mask; this is exact for defined shift
      counts, while negative results are already undefined as shift counts.
      The `% 64ll` regression checks legacy and verified-backend DWARF. The
      focused inline-expression test and full native-Windows `test-debug-info`
      pass; the focused Linux ASan/UBSan inline-expression test passes.
- [ ] General computed 64-bit count division/remainder, non-frame-backed
      operands, and broader optimized multi-piece locations remain open.

## RCC continuation (2026-10-22, computed signed wide shift-count remainder)

- [x] Extend signed power-of-two remainder lowering to bounded pure computed
      operands by matching the optimizer's duplicated sign-corrected expression
      trees. The `(count + 1ll) % 64ll` regression supplies nonzero high-word
      count values and evaluates defined results across legacy and verified
      i686 DWARF. Focused and full native-Windows `test-debug-info` plus focused
      Linux ASan/UBSan inline-expression tests pass.
- [ ] General computed 64-bit count division/remainder, non-frame-backed
      operands, and broader optimized multi-piece locations remain open.

## RCC continuation (2026-10-23, i686 unsigned 64-bit constant divmod DWARF)

- [x] Extend frame-backed i686 `unsigned long long` division and remainder
      constants from `2^30 + 1` through `INT32_MAX`. Use one-bit long-division
      digits with a high-bit test and a signed comparison only for nonnegative
      partial dividends, preserving exact quotient/remainder pieces without
      signed-32 overflow. Evaluate ten full-width boundary inputs for
      `/1073741825` and `%2147483647` in legacy and verified DWARF. Focused
      native-Windows and Linux inline-expression tests, full native-Windows
      `test-debug-info`, and GCC ASan/UBSan full `test-debug-info` pass.
- [ ] TLS or volatile/reference/atomic operands, wider computed i686 64-bit
      division/remainder expressions, and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-23, i686 unsigned 64-bit divmod through UINT32_MAX)

- [x] Extend frame-backed i686 `unsigned long long` constant division and
      remainder through `UINT32_MAX`. Preserve the 33rd partial-dividend bit
      across each one-bit long-division step, and use sign-biased signed
      comparison for exact unsigned `DW_OP_ge` behavior when the divisor exceeds
      `INT32_MAX`. Evaluate ten full-width boundary values for `/2147483649`
      and `%4294967295` in legacy and verified output. Native-Windows and Linux
      focused inline-expression tests, Linux full `test-debug-info`, and GCC
      ASan/UBSan full `test-debug-info` pass.
- [x] Preserve frame-backed i686 unsigned 64-bit division/remainder by
      power-of-two constants from `2^32` through `2^62` as exact low/high
      `DW_OP_piece` values. Verify division and remainder at `2^32`, division
      at `2^40`, remainder at `2^62`, and ten full-width boundary dividends in
      legacy and verified output. Focused and full native-Windows
      `test-debug-info` pass.
- [x] Emit i686 unsigned-64 global division/remainder for non-power-of-two
      constants above `INT64_MAX` (tested with `0x8000000000000001` and
      `UINT64_MAX`). The quotient uses exact unsigned high/low comparisons;
      the remainder subtracts both words and propagates the low-word borrow.
      Verify 11 boundary dividends, initialized globals, and exactly three
      division or eight remainder relocations in legacy and verified objects.
      Focused `test-debug-info-inline-expression` and full `test-debug-info`
      pass.
- [x] Emit i686 unsigned-64 division/remainder for non-power-of-two constants
      in `(UINT32_MAX, INT64_MAX]` with an unrolled bitwise long-division
      expression over two 32-bit remainder words. Use sign-biased unsigned
      comparisons and propagate the low-word borrow. Regressions cover
      `0x100000001`, `0x4000000000000001`, and `INT64_MAX` against 11 boundary
      dividends and initialized globals, with exactly two source relocations
      in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Preserve frame-backed i686 unsigned-64 division/remainder with a runtime
      unsigned-64 divisor for read-only inline-parameter expressions. Keep the
      dividend/divisor words and quotient bits on the DWARF stack, compare words
      with sign bias, and propagate low-word borrow. Evaluate 11 boundary
      dividends against nine nonzero divisors (including high-bit and maximum
      values) for both `/` and `%` in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Load an ordinary global i686 unsigned-64 runtime divisor alongside a
      frame-backed dividend with exactly two `ABS32U` relocations. Verify the
      initialized high-bit divisor and the 11-by-9 division/remainder boundary
      matrix in legacy and verified objects; focused inline-expression and
      full `test-debug-info` gates pass.
- [x] Load ordinary global unsigned-64 dividend and divisor operands, both
      separately with a frame-backed counterpart and together, using exactly
      two `ABS32U` relocations per global operand. Verify both initialized
      globals and the 11-by-9 division/remainder boundary matrix in legacy and
      verified objects.
- [ ] TLS or volatile/reference/atomic operands, wider computed i686 64-bit
      division/remainder expressions, and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-23, Windows MinGW structural NTTP host ABI)

- [x] Keep the structural inherited member-function-pointer NTTP comparison
      ABI-correct on Windows: MinGW x86-64 is LLP64 and emits an `x`-width
      ptrdiff adjustment for host GCC, while RinOS x86-64 remains LP64 and
      correctly emits the target `l`-width adjustment. Separate the host-GCC
      oracle symbol from the RinOS target symbol. The complete `test-cxx`
      target list passed on Windows when split at the separately passing
      structural-array-NTTP target; no compiler-generated target ABI spelling
      was changed.

## RCC continuation (2026-10-23, dependent class-template cv substitution)

- [x] Preserve `const`/`volatile`/`restrict`/`atomic` qualifiers when a
      dependent class-template-id is instantiated in both parser deduction
      and AST function-template specialization. A function taking
      `volatile Box<T>*` now accepts a volatile specialization and deduces
      `T`; the regression also exercises `volatile std::atomic<T>*`.
      `test-atomic-builtins` passes both architectures at `-O0`/`-O2`, runs
      the AMD64 objects, and `test-cxx-function-template-overloads` passes.
- [x] Use the standard `atomic<T>`-constrained signatures for all non-member
      `<atomic>` operations. Route dependent class-template substitution
      through the parser's partial-specialization selector, so function
      templates instantiate `atomic<T*>` rather than the primary `atomic<T>`;
      pointer constructors, arithmetic, and free-function APIs compile and
      execute again. `test-atomic-builtins` passes i686/AMD64 at `-O0`/`-O2`,
      AMD64 execution, and the GCC C++20 syntax oracle.
- [ ] Full `<atomic>` type constraints/traits, remaining constructor/API
      overloads, and C++20 wait/notify operations remain open.

## RCC continuation (2026-10-23, C++20 atomic typedef aliases)

- [x] Provide the standard `std::atomic_bool`, integer, and character aliases,
      including C++20 `std::atomic_char8_t`. Add runtime checks for each alias
      and resolve qualified namespace-scope typedef names through direct and
      aliased namespace qualifiers. `test-atomic-builtins` passes i686/AMD64
      generation at `-O0`/`-O2`, GCC C++20 standard-library syntax checking,
      and AMD64 runtime execution at both optimization levels;
      `test-cxx-qualified-namespaces`, `test-cxx-alias-templates`, and
      `test-cxx-using` pass.
- [ ] Atomic type constraints/traits, remaining constructor/API overloads,
      and C++20 wait/notify operations remain open.

## RCC continuation (2026-10-23, C++ atomic non-member RMW API)

- [x] Provide the standard non-member `atomic_fetch_add/sub/and/or/xor` and
      explicit-order overloads, including element-scaled pointer add/sub.
      Runtime checks verify previous values and resulting states for all
      operations. `test-atomic-builtins` passes both target widths at `-O0` and
      `-O2`, executes AMD64 objects at both levels, and passes the GCC C++20
      standard-library syntax oracle.
- [ ] Atomic type constraints/traits, remaining constructor/API overloads,
      and C++20 wait/notify operations remain open.

## RCC continuation (2026-10-23, C++20 atomic_flag test free functions)

- [x] Add `std::atomic_flag_test` and `std::atomic_flag_test_explicit` with
      the standard const-volatile object parameter and sequentially-consistent
      default. Exercise clear/set state transitions and relaxed/acquire reads
      in `test-atomic-builtins`; i686/AMD64 `-O0`/`-O2` generation, AMD64
      execution at both optimization levels, and GCC C++20 syntax checking
      pass.
- [ ] Atomic type constraints/traits, remaining constructor/API overloads,
      and C++20 wait/notify operations remain open.

## RCC continuation (2026-10-23, C++20 scalar `atomic_ref` operations)

- [x] Add `std::atomic_ref` operations for supported scalar types: lock-free
      and required-alignment properties, load/store/conversion/assignment,
      exchange, weak/strong compare-exchange, integer fetch arithmetic and
      bitwise RMW, floating fetch-add/sub, and element-scaled pointer
      fetch-add/sub. Add integer/float arithmetic compound assignments,
      integer bitwise compound assignments, and pointer compound/increment
      operators. Runtime regressions cover successful CAS, memory-order
      overloads, signed wraparound, floating updates, bitwise transitions,
      and pointer scaling/operator results. `test-atomic-builtins` passes
      i686/AMD64 generation at `-O0` and `-O2`, AMD64 execution at both
      optimization levels, and the GCC C++20 syntax oracle.
- [x] Enforce the trivially-copyable type requirement when instantiating
      `std::atomic<T>` and `std::atomic_ref<T>`. Compile-fail regressions verify
      both diagnostics on i686 and AMD64.
- [x] Constrain generic `atomic_ref<T>` arithmetic and bitwise fetch/compound
      members, plus generic `atomic<T>` bitwise fetch/compound members, with
      trailing requires-expressions over the existing supported helper
      overloads and integer atomic builtins. The requirements reject aggregates
      and user-defined overloaded arithmetic/bitwise operators, retain integer
      bitwise operations and float/double `atomic_ref` add/sub, and cover
      volatile `atomic<T>` bitwise overloads. `test-atomic-builtins` passes
      i686/AMD64 generation at `-O0` and `-O2`, AMD64 execution at both levels,
      and the GCC C++20 syntax oracle.
- [x] Constrain generic `atomic<T>` `fetch_add/sub` and `+=/-=` to the existing
      supported integer and float/double helper overloads. Requires-expression
      regressions retain integer/floating operations and reject `bool`,
      aggregates, and user-defined arithmetic operators. `test-atomic-builtins`
      passes i686/AMD64 generation at `-O0` and `-O2`, AMD64 execution at both
      levels, and the GCC C++20 syntax oracle.
- [x] Restrict primary `atomic<T>` prefix/postfix increment and decrement to
      the integer helper overloads, including volatile objects. Extend unary
      operator rewriting to materialize constrained member-function-template
      candidates before selecting the operator call, without changing the
      argument list used for non-member operators. C++20 requires-expression
      regressions accept integer operators and reject float, `bool`, and
      aggregates; runtime checks exercise all four operators on ordinary and
      volatile integers. `test-atomic-builtins` passes i686/AMD64 generation at
      `-O0` and `-O2`, AMD64 execution at both levels, and the GCC C++20 syntax
      oracle.
- [ ] Remaining type/API constraints, broader `<atomic>` constructor/API
      coverage, and C++20 wait/notify operations remain open. Dependent
      evaluation is available for the implemented boolean type-trait builtins;
      traits not yet implemented by RCC and broader member-constraint/SFINAE
      cases remain open.
- [x] Support generic `atomic_ref<T>` load/store/exchange/compare-exchange for
      4-byte and 8-byte aggregates. The i686 8-byte path uses `CMPXCHG8B`; both
      target backends use their existing atomic-width lowering. Runtime checks
      exercise the full 8-byte operation sequence, including successful and
      failed compare-exchange, plus 4-byte load/exchange/compare-exchange. A
      12-byte aggregate remains rejected with a diagnostic.
      `test-atomic-builtins` passes i686/AMD64 generation at `-O0`/`-O2`,
      AMD64 execution at both levels, and GCC C++20 syntax checking.

## RCC continuation (2026-10-23, in-class trailing requires clauses)

- [x] Parse and retain trailing requires clauses on in-class member function
      templates, combining them with any template-head constraint before
      overload resolution. Regressions prove constrained class-template member
      calls are available for `Box<int>` and unavailable for `Box<char>`.
      i686/AMD64 code generation passes at `-O0` and `-O2`; AMD64 host-runtime
      execution passes at both levels, and GCC C++20 syntax checking passes.

## RCC continuation (2026-10-23, const atomic type constraint)

- [x] Reject `std::atomic<const T>` during concrete class-template
      instantiation: the const-qualified value cannot satisfy the atomic
      copy-assignment requirement. Compile-fail regressions assert the
      diagnostic on i686 and AMD64 as part of `test-atomic-builtins`.
- [ ] Other copy/move construction and assignment constraints, complete
      type-trait support, and C++20 wait/notify operations remain open.

## RCC continuation (2026-10-23, invalid atomic object types)

- [x] Reject `std::atomic` specializations over `void`, function, and array
      types before instantiating member declarations. Dual-architecture
      compile-fail regressions check the copy/move object-type diagnostic.
- [ ] General copy/move constructibility and assignability constraints,
      complete type-trait support, and C++20 wait/notify operations remain open.

## RCC continuation (2026-10-23, atomic deleted special-member constraints)

- [x] Preserve the standard trivially-copyable classification for classes
      whose copy/move special member is deleted, while rejecting
      `std::atomic<T>` when a copy/move constructor or assignment is explicitly
      deleted. The dual-architecture invalid fixture covers deleted copy and
      move construction and assignment with type-specific diagnostics. The
      host GCC C++20 oracle confirms all four corresponding type traits are
      false; `test-atomic-builtins` passes i686/AMD64 generation at `-O0` and
      `-O2`, AMD64 runtime at both levels, and valid GCC C++20 syntax checks.
- [x] Include user-provided copy/move assignment operators in the
      trivially-copyable check. Propagate explicitly deleted copy constructors
      and assignments through implicit/defaulted base, data-member, and fixed
      array-member subobjects before accepting `std::atomic<T>`. Also reject a
      nested subobject whose declared move member suppresses its implicit copy
      operations. GCC 13.2 rejects the same nested cases;
      `test-atomic-builtins` checks named RCC diagnostics for both target
      architectures.
- [ ] Complete subobject constructibility/assignability beyond these explicit
      deleted-copy cases, including access control, overload viability, and all
      implicit special-member deletion interactions. Complete type-trait
      support and C++20 `atomic::wait`/`notify` also remain open.

## RCC continuation (2026-10-23, generic `std::atomic<T>` aggregate operations)

- [x] Keep `void`, function, and reference types outside the trivially-copyable
      domain used by `std::atomic_ref`, and reject reference specializations of
      `std::atomic`. Isolated negative fixtures verify `std::atomic<unsigned&>`
      and `std::atomic_ref<void>`, function, and reference specializations on
      both i686 and AMD64.

- [x] Route the primary `std::atomic<T>` load/store/exchange/compare-exchange
      implementation through the generic builtins and preserve integer RMW
      helpers without instantiating scalar-only operations for aggregate `T`.
      Enable exact-type aggregate parameter forwarding and member copying in
      constructor lowering. Runtime checks cover 4-byte and 8-byte aggregates
      for load/store/exchange/compare-exchange; `test-atomic-builtins` passes
      i686/AMD64 `-O0`/`-O2` generation, AMD64 execution at both levels, and
      GCC C++20 syntax checking.
- [x] Extend the same bounded trivial-copy path to plain C++ unions, including
      active-member aggregate initialization and aggregate-byte copying in
      both code generators. `std::atomic<Union>` and `std::atomic_ref<Union>`
      load/store/exchange/compare-exchange regressions pass for i686/AMD64
      `-O0`/`-O2` generation and AMD64 runtime at both levels; the inline
      aggregate C++ regression target also passes.
- [x] Follow the C++20 `std::atomic<T>` default-constructor rule by
      value-initializing the contained object. Cover trivial 8-byte aggregates
      with all default-member initializers and with an omitted member that must
      be zero-initialized. `test-atomic-builtins` passes both target widths and
      optimization levels, executes AMD64 at `-O0`/`-O2`, and agrees with the
      GCC C++20 syntax oracle; `test-cxx-inline-aggregates` remains green.
- [x] Support the validated C++20 union default-member-initializer subset in
      the shared aggregate parser and value-initialization path: one active
      member initialized with either `= expr` or braced syntax. Diagnose more
      than one initialized union member. Runtime checks cover direct
      default-initialization and confirm an explicitly selected alternate
      variant suppresses the default member initializer. `test-atomic-builtins`
      covers both target widths, both optimization levels, AMD64 runtime at
      `-O0`/`-O2`, dual-target invalid diagnostics, and the GCC C++20 syntax
      oracle; `test-cxx-inline-aggregates` passes.
- [ ] Remaining aggregate/union default-member-initializer corners, widths
      above eight bytes, broader implicit special-member viability, complete
      type traits, and C++20 wait/notify remain open. Union atomic operations
      remain limited to the validated trivial-copyable subset.

## RCC continuation (2026-10-23, builtin feature queries)

- [x] Implement the `__has_builtin` preprocessing operator and make it visible
      to `defined()`. Report all builtin names currently dispatched by the C/C++
      frontends, including generic atomics and the supported MMX subset, while
      leaving unsupported and unknown names unadvertised.
      `test-preprocessor-has-builtin` covers C/C++, both target widths,
      positive/negative probes,
      malformed arguments, and preprocessing the libcxx type-traits header;
      the existing `__has_include`, `__has_cpp_attribute`, and C++ feature-gate
      tests also pass.
- [x] Directly compile the complete libcxx `type_traits.h` include through
      `test-cxx-libcxx-type-traits-direct`: RCC generates both i686 and AMD64
      objects, and GCC C++20 accepts the same fixture. This verifies the full
      header's direct parse/semantic-compile path; it does not claim complete
      instantiation or conformance of every trait.
- [x] Implement `__is_constructible(T, Args...)` for scalar conversions,
      reference binding, modeled public constructors (including explicit,
      deleted, and inaccessible best matches), and C++20 aggregate
      initialization. Preserve dependent type packs through substitution and
      advertise the builtin through `__has_builtin`. The focused fixture and
      instantiated `std::is_constructible` checks pass on i686/AMD64 and
      against GCC; the direct libcxx header gate passes.
- [ ] Inherited constructor-template resolution and remaining constrained
      constructor interactions remain open. Direct constructor templates and
      ordinary inherited constructors, including access/deletion in best-match
      selection, are covered below; implicit copy/move special-member viability,
      assignment traits, nothrow-constructibility, and remaining
      target-specific builtin-query cases also remain open.
      Continue instantiating real traits and comparing with GCC rather than
      treating header parsing as complete support.
- [x] Define target-accurate `__SIZE_TYPE__`, `__PTRDIFF_TYPE__`, pointer-sized,
      fixed-width, fast-width, character, integer-limit, `__SIZEOF_*`, and
      byte-order macros. A shared preprocessor fixture checks the exact common
      and ILP32/LP64 values from both C and C++20 freestanding sources; existing
      type-width and UTF-character checks remain. The native-Windows
      `test-preprocessor-standard-macros` gate passes.
- [x] Implement the C++ `__is_trivially_copyable(T)` type intrinsic using RCC's
      semantic classifier. C++20 static assertions cover scalar, pointer,
      array, cv-qualified, ordinary-constructor, union, user-copy-constructor,
      defaulted/user-provided destructor, copy-constructor default arguments,
      reference-member, polymorphic, void, function, and reference types; the
      C++ regression aggregate runs both target widths and a GCC syntax oracle.
      Reference members do not invalidate a class's trivial copying.
- [x] Implement the C++ `__is_trivially_destructible(T)` intrinsic, including
      bounded arrays, references, deleted/inaccessible destructors, virtual
      destructors, and member/base destructor propagation. The GCC C++20
      `std::is_trivially_destructible` oracle and RCC i686/AMD64 checks pass;
      aggregate cleanup, inline aggregate, and atomic regression suites pass.
- [x] Implement `__has_virtual_destructor(T)` with inherited virtual-destructor
      detection, and advertise the implemented C++ type-trait queries through
      `__has_builtin`. RCC i686/AMD64 trait tests, the GCC C++20 standard-trait
      oracle, and the C++ preprocessing feature-query suite pass.
- [x] Implement C++17 `__is_aggregate(T)` for complete aggregate classes,
      unions, and bounded arrays, using the shared aggregate semantic
      classifier. Tests cover C++20 default-member-initializers and public
      bases, private members, user-declared constructors, virtual functions,
      pointers, and enums; RCC i686/AMD64, GCC C++20, and `__has_builtin`
      checks pass.
- [x] Implement two-type `__is_base_of(Base, Derived)` using class ancestry
      independent of access or ambiguity, with identical-class and non-class
      cases checked. RCC i686/AMD64, GCC C++20 `std::is_base_of`, and the
      `__has_builtin` query suite pass.
- [x] Implement the C++ `__underlying_type(Enum)` type-name builtin for fixed,
      defaulted, and opaque enums, including use as a template argument. Both
      target widths and GCC's exact-type oracle pass; invalid non-enum operands
      produce a diagnostic, and `__has_builtin` reports the implementation.
- [x] Implement the C++ type-category intrinsics `__is_enum(T)`,
      `__is_class(T)`, `__is_union(T)`, `__is_final(T)`, `__is_abstract(T)`,
      `__is_polymorphic(T)`, and `__is_empty(T)`. Regressions cover positive,
      negative, pointer, class-key, incomplete-class, pure-virtual, inherited
      polymorphism, base/member/static/bitfield emptiness cases on i686/AMD64
      and pass GCC's C++20 syntax oracle.
- [x] Preserve dependent operands for all implemented boolean C++ type-trait
      builtins and reevaluate them after template type substitution. Static
      values, two-type `__is_base_of`, pointer operands, and constrained
      overload selection are covered by the RCC/GCC type-trait regression on
      i686 and AMD64.

## RCC continuation (2026-10-23, C++ `__is_trivial` builtin)

- [x] Implement `__is_trivial(T)` using the existing trivial-copy semantic
      classifier plus default-constructor, base/member, virtual-subobject, and
      default-member-initializer rules. Preserve dependent operands through
      template substitution and advertise the builtin with `__has_builtin`.
      Regressions compare against GCC C++20 `std::is_trivial` for scalar,
      array, enum, union, deleted/defaulted/user-provided constructors,
      reference members, bases, virtual classes, and constraints; RCC tests
      pass for i686 and AMD64.
- [ ] Constrained and out-of-class-defaulted special-member eligibility and
      broader constructibility/assignability traits remain open; the
      `libcxx/type_traits.h` direct-compilation blocker is not resolved by this
      builtin alone.

## RCC continuation (2026-10-23, C++ `__is_standard_layout` builtin)

- [x] Implement `__is_standard_layout(T)` for scalar and array types, classes,
      and unions. Check member types/access, virtual bases/functions, unique
      base subobjects and standard-layout base types, declaration ownership of
      data members, and the C++20 zero-offset member/base type rule. Defer
      dependent operands through template substitution and advertise the
      builtin with `__has_builtin`.
      The GCC C++20 oracle and RCC i686/AMD64 tests cover positive and negative
      base/member/access/reference/union cases plus dependent constraints.
- [ ] Remaining broader C++ type-trait semantics remain open. Direct inclusion
      of the complete `libcxx/type_traits.h` is covered by the checked
      `test-cxx-libcxx-type-traits-direct` item below.

## RCC continuation (2026-10-23, C++ `__is_pod` builtin)

- [x] Implement the deprecated C++ POD type trait as a recursive semantic
      check over scalar, array, class, and union types, requiring trivial and
      standard-layout properties and POD data members. Preserve dependent
      operands through specialization and advertise the builtin through
      `__has_builtin`; GCC C++20 and RCC dual-target tests cover positive and
      negative scalar, array, enum, union, constructor, access, and member cases.
- [ ] Broader constructibility/assignability trait semantics remain open;
      complete-header direct compilation is covered by the checked fixture.

## RCC continuation (2026-10-23, GNU `__has_trivial_destructor` builtin)

- [x] Implement the GNU legacy trivial-destructor query separately from
      `__is_trivially_destructible`: test destructor triviality independent of
      access or deletion status while recursively checking virtual and
      nontrivial base/member destructors. Preserve dependent operands,
      advertise it through `__has_builtin`, and compare RCC's i686/AMD64
      results to GCC's builtin.
- [ ] Remaining GNU special-member and C++ constructibility/assignability
      intrinsics remain open; complete-header direct compilation is covered by
      the checked fixture.

## RCC continuation (2026-10-23, C++ `__is_trivially_constructible` builtin)

- [x] Implement variadic `__is_trivially_constructible(T, Args...)` for the
      currently modeled scalar conversions, references, bounded arrays,
      aggregate classes/unions, and trivial defaulted copy/move constructors.
      Preserve dependent type operands and directly expanded type parameter
      packs, include object-destructor accessibility/triviality, and advertise
      the builtin through `__has_builtin`. RCC i686/AMD64 tests compare against
      GCC C++20 `std::is_trivially_constructible` for conversions, references,
      defaulted/deleted/inaccessible constructors, C++20 parenthesized
      aggregate and union initialization, destructor cases, and dependent packs.
      Separate C++17 regressions verify that parenthesized aggregate/array
      initialization remains unavailable before C++20 on both targets and
      agrees with GCC's C++17 standard-trait results.
- [ ] Full overload resolution for constructor templates, inherited and
      constrained constructors, complete implicit/out-of-class-defaulted
      special-member rules, and broad instantiation of `libcxx/type_traits.h`
      remain open. The former direct-probe blocker in dependent function-
      template call substitution for `decltype`/SFINAE (`is_convertible` and
      `is_nothrow_convertible`) is implemented and covered by the dedicated
      direct-header fixture. Continue the broader work from the first
      diagnostic in an instantiated trait; full-header direct compilation is
      separately checked above.

## RCC continuation (2026-10-23, global allocation operator declarations)

- [x] Parse namespace-scope `operator new`, `operator new[]`,
      `operator delete`, and `operator delete[]` declarations/definitions,
      including sized and placement forms with pointer return types. Compile
      both RinOS targets and check the same overload set with GCC C++20.
- [ ] Enforce all standard allocation-function signature restrictions and
      resolve user-defined allocation/deallocation overloads from `new` and
      `delete` expressions, including class-specific forms.
  - [x] Validate global `operator new`/`new[]` return type, target `std::size_t`
        first parameter, absence of a default on that parameter, and global
        namespace/non-static placement. Validate global `operator delete`/
        `delete[]` return and first-parameter types. The focused target rejects
        each invalid declaration on i686 and AMD64, accepts the valid overload
        set, and agrees with GCC C++20 diagnostics.
  - [x] Resolve scalar global `operator new` and `operator delete` from
        `new`/`delete` expressions alongside the implicit RinOS allocator
        candidate. Carry the selected deallocator through scalar destruction
        and exception cleanup. The regression executes replacement operators
        with both native code generation paths and checks the verified backend
        for i686/AMD64 at `-O0` and `-O2`; specialized array forms remain open.
  - [x] Resolve global `operator new[]`/`operator delete[]` for uninitialized
        arrays of scalar elements without cookies. Exercise replacement
        operators and the implicit RinOS fallback on i686 and AMD64, run the
        native x64 output, and verify both targets at `-O0`/`-O2` with no
        verified-backend fallback.
  - [x] Resolve global `operator new[]`/`operator delete[]` for explicitly
        initialized scalar arrays without placement arguments. Verify the
        requested byte extent, explicit values, zero-initialized omitted
        elements, GCC C++20, legacy i686/AMD64 generation and AMD64 execution,
        and verified i686/AMD64 `-O0`/`-O2` with zero fallback in
        `test-cxx-global-allocation-operators`.
  - [x] Resolve direct class-specific scalar `operator new`/`operator delete`
        before global replacements or the implicit RinOS allocator. Treat
        class-scope allocation functions as implicitly static, validate their
        signatures, and cover out-of-class definitions plus global-fallback
        suppression. `test-cxx-global-allocation-operators` passes GCC C++20,
        legacy i686/AMD64 generation and x64 execution, and verified backend
        i686/AMD64 at `-O0`/`-O2` with zero fallback.
  - [x] Extend class-specific overload resolution to uninitialized arrays of
        trivial class elements with no cookie or element initialization. Check
        the passed byte extent, selected `operator new[]`/`operator delete[]`,
        GCC C++20, legacy i686/AMD64 generation and x64 execution, and verified
        i686/AMD64 at `-O0`/`-O2` with zero fallback. The same fixture confirms
        that a class without member allocation operators uses the implicit
        RinOS allocator pair. Constructor/destructor arrays with cookies are
        covered by the follow-up below; placement arguments and constructor-
        failure matching placement delete remain open.
  - [x] Resolve public class-specific scalar and array allocation/deallocation
        operators inherited by an unambiguous derived class. Preserve triviality
        when a base is trivial and propagate nontrivial base lifetime state;
        verify derived byte extents and selected operators at runtime. GCC C++20
        and legacy i686/AMD64 generation plus x64 execution pass; verified
        i686/AMD64 `-O0`/`-O2` emits 11 functions with zero fallback. Conflicting
        direct-base scalar/array lookup is diagnosed as ambiguous; a public
        using-declaration selecting one direct base resolves both forms and is
        runtime-checked. RCC i686/AMD64 diagnostics and execution agree with GCC
        C++20. Multiple using-declarations retain same-signature candidates and
        diagnose overload ambiguity, matching GCC. Distinct placement overloads
        are exercised in the placement-allocation coverage below.
        Repeated and virtual diamond allocation lookup passes GCC and both RCC
        target generations. Virtual-base object lifetime lowering and bases
        requiring constructor/destructor lowering remain open. Private
        using-declarations reject external allocation.
  - [x] Parse placement allocation arguments and pass them into global and
        class-specific scalar and plain trivial-array operator new lookup.
        Verify exact scalar/array byte extents, overload selection from two
        public base using-declarations, global placement operators, x64
        execution, GCC C++20, and verified i686/AMD64 -O0/-O2 with zero
        fallback. A placement call with no matching operator is diagnosed
        instead of silently using the RinOS allocator.
  - [x] Distinguish parenthesized type-id new forms such as new (int)(value)
        from placement argument lists; verify GCC C++20, x64 execution, and
        verified i686/AMD64 -O0/-O2 with zero fallback.
  - [x] Add an allocator-free Windows entry harness for allocation fixtures
        that define strong `rin_malloc`/`rin_free` hooks, while retaining the
        generic harness's allocator defaults for other tests. The complete
        `test-cxx-global-allocation-operators` target passes.
  - [x] Resolve ordinary one-argument global and class-specific
        `operator new[]`/`operator delete[]` for cookie-bearing class arrays
        with `noexcept` default constructors and destructors. Pass the
        cookie-inclusive byte extent to allocation and the original allocation
        address to deallocation in legacy codegen and typed SSA. Verify i686/
        AMD64 generation, AMD64 runtime behavior, and verified backend output
        with zero fallback for both targets at `-O0` and `-O2` in
        `test-cxx-global-allocation-operators`.
  - [x] Pass one pointer placement argument to a class-specific
        `operator new[]` for a cookie-bearing array with `noexcept` default
        construction/destruction. Verify the pointer and cookie-inclusive size,
        normal `delete[]`, i686/AMD64 generation, AMD64 runtime, and verified
        backend output with zero fallback for both targets at `-O0` and `-O2`
        in `test-cxx-global-allocation-operators`.
  - [x] Pass multiple pointer placement arguments to a class-specific
        `operator new[]` for cookie-bearing arrays with `noexcept` default
        construction/destruction. Verify two arguments and six arguments (the
        sixth passed on the AMD64 stack), their values and cookie-inclusive
        size, normal `delete[]`, i686/AMD64 generation, AMD64 runtime, and
        verified output with zero fallback for both targets at `-O0` and `-O2`
        in `test-cxx-global-allocation-operators`.
  - [x] Resolve global `operator new[]`/`operator delete[]` for cookie-bearing
        class arrays with explicit per-element constructor arguments. Verify
        constructor values, destructor count, cookie-inclusive allocation size,
        GCC C++20, legacy i686/AMD64 generation and AMD64 execution, plus
        verified i686/AMD64 output at `-O0`/`-O2` with zero fallback in
        `test-cxx-global-allocation-operators`.
  - [x] Resolve class-specific `operator new[]`/`operator delete[]` for cookie-
        bearing class arrays with explicit per-element constructor arguments.
        Verify constructor values, destructor count, cookie-inclusive size,
        strict GCC C++20, legacy i686/AMD64 generation and AMD64 execution, and
        verified i686/AMD64 output at `-O0`/`-O2` with zero fallback in
        `test-cxx-global-allocation-operators`.
  - [x] For cookie-bearing class arrays with class-specific placement
        allocation taking pointer/integer arguments, preserve the evaluated
        values through constructor failure, destroy completed elements in
        reverse order, then invoke the matching placement `operator delete[]`.
        Cover both explicit per-element initializers and default construction;
        the runtime checks the cookie-inclusive extent, both delete arguments,
        destructor order, i686/AMD64 legacy generation and AMD64 execution,
        plus verified i686/AMD64 `-O0`/`-O2` with zero fallback and x64 runtime
        execution in `test-cxx-global-allocation-operators`.
  - [x] Pass scalar `float`/`double` placement arguments through the AMD64 SSE
        argument registers and stack overflow area, independently of integer registers.
        The constructor-failure cleanup thunk restores the same argument mix;
        the regression also exhausts both register classes. Verify AMD64 legacy
        runtime, i686/AMD64 verified output at `-O0`/`-O2` with zero fallback,
        i686 legacy object generation, and strict GCC C++20 in
        `test-cxx-global-allocation-operators`.
  - [x] Lower i686 signed/unsigned 64-bit placement arguments as low/high
        32-bit ABI words in both the allocation call and matching constructor-
        failure cleanup thunk. The regression preserves `0x1122334455667788`
        among floating and integer register/stack arguments. The full
        `test-cxx-global-allocation-operators` gate passes with verified i686
        and AMD64 output at `-O0`/`-O2`, zero fallback, verified AMD64 runtime,
        and strict GCC C++20. The 32-bit verified runner executes the i686
        objects at `-O0` and `-O2` under WSL; legacy i686 execution also links
        and runs there with the existing i686 exception runtime. The legacy
        AMD64 runtime passes as well.
  - [x] Lower trivial aggregate-by-value placement arguments through i686 and
        AMD64 allocation calls and matching constructor-failure cleanup thunks.
        Cover a mixed SSE/INTEGER 16-byte aggregate and a 24-byte MEMORY-class
        aggregate; verify saved cleanup-record sizing, argument preservation,
        two completed-element destructors in reverse order, and matching
        placement delete arguments. `test-cxx-global-allocation-operators`
        passes legacy i686/AMD64 runtime and verified i686/AMD64 generation
        with zero fallback at `-O0`/`-O2`; WSL executes all four verified
        objects, and strict GCC C++20 syntax plus the Windows gate pass.
  - [x] Extend aggregate-by-value placement allocation and constructor-failure
        cleanup to aggregates with explicit 16-byte alignment. Preserve the
        aggregate's alignment in allocation temporaries and cleanup records.
        `test-cxx-global-allocation-operators` passes legacy i686/AMD64 runtime,
        verified i686/AMD64 output with zero fallback at `-O0`/`-O2`, verified
        i686 and AMD64 runtime (including explicit AMD64 `-O2` execution), and
        strict GCC C++20 syntax on Windows and POSIX hosts.
  - [x] Extend x86_64 aggregate return handling to explicitly 16-byte-aligned
        mixed SSE/INTEGER aggregates. Preserve both ABI return registers in the
        legacy caller and lower the return/caller through verified SSA.
        `test-cxx-global-allocation-operators` passes legacy i686/AMD64 runtime,
        verified i686/AMD64 `-O0`/`-O2` with zero fallback, verified AMD64
        `-O0`/`-O2` runtime, alignment/size static assertions, and strict GCC
        C++20 syntax on Windows and POSIX hosts.

## RCC continuation (2026-10-23, C++ declaration attributes in linkage blocks)

- [x] Consume standard `[[noreturn]]` on declarations inside `extern "C"`
      linkage blocks and preserve its function-declaration flag. Regression
      coverage compiles the attributed definition for i686 and AMD64 and
      checks the same source with GCC C++20.
- [x] Preserve `[[deprecated]]` on block-scope variables, including every
      declarator in a comma-separated declaration. `test-cxx-deprecated` checks
      the six expected diagnostics on both target widths, GCC accepts the same
      C++20 source, the i686 COFF architecture check passes, and the generated
      AMD64 code executes successfully.
- [x] Preserve `[[deprecated]]` on local function declarations, class members,
      and instantiated function templates; compare their diagnostics against
      GCC C++20 in the focused fixture.
- [x] Carry `[[deprecated]]` forward to later function redeclarations with the
      same ABI symbol. Cover attribute-before-definition, attribute-after-
      definition followed by an unannotated redeclaration, and a call before
      the attribute declaration that must remain unmarked.
- [x] Parse `[[deprecated]]` after the class-key, preserve it on ordinary,
      primary, explicit-specialization, and partial-specialization classes, and
      diagnose uses of the selected class. The fixture compares GCC diagnostics
      and runs generated code; existing specialization and dependent-pack tests
      remain green.
- [ ] Other class-template redeclaration and attribute-ordering cases remain
      open.

## RCC continuation (2026-10-23, GNU `__has_trivial_constructor` builtin)

- [x] Implement the GNU legacy trivial-constructor query independently of
      whether default construction is accessible or deleted. Recursively check
      arrays, bases, and non-static data members, while preserving GCC's
      behavior for reference members and deleted member constructors. Preserve
      dependent operands, advertise the builtin through `__has_builtin`, and
      compare RCC's i686/AMD64 results against GCC C++20.
- [ ] Remaining GNU special-member and C++ constructibility/assignability
      intrinsics remain open; complete-header direct compilation is covered by
      the checked fixture.

## RCC continuation (2026-10-30, GNU `__has_nothrow_constructor` builtin)

- [x] Implement and advertise `__has_nothrow_constructor(T)` with dependent
      type evaluation after substitution. Match GCC's legacy semantics for
      arrays, inaccessible/deleted trivial constructors, defaulted
      subobject/base constructors, default member initializers, and
      potentially-throwing constructor exception specifications. Do not fold
      destructor exceptions or evaluation of constructor default arguments
      into this legacy query. The focused i686/AMD64 RCC gate and strict GCC
      C++20 syntax oracle pass.
- [ ] Broader GNU special-member, constructibility, and assignability
      conformance remains open.

## RCC continuation (2026-10-23, class typedefs and function-type packs)

- [x] Parse class-scope `typedef` declarations, including multiple declarators
      and typedefs in class-template partial specializations. RCC i686/AMD64
      compilation and the GCC C++20 syntax oracle pass.
- [x] Implement and advertise `__is_function(T)`, preserving dependent
      operands until substitution. Cover function, variadic-function, and
      function-pointer operands, plus fixed and C-variadic function-type pack
      matching in class partial specializations. RCC i686/AMD64 and GCC C++20
      regressions pass.
- [x] Defer dependent explicit function-template calls in `decltype` until
      template substitution and apply SFINAE when overload resolution fails.
      The shared conversion ranker now accounts for public non-explicit
      converting constructors, the noexcept evaluator accounts for the
      selected constructor, and the dedicated `type_traits` direct fixture
      covers viable, explicit-only, nothrow, and potentially-throwing
      conversions for i686/AMD64 and GCC C++20.

## RCC continuation (2026-10-24, class partial-specialization pack declarators)

- [x] Preserve the declarator parser's consumed `...` marker when recording a
      class partial-specialization's argument-pack expansion. Cover zero- and
      one-element tails, pointer-pattern ordering, constrained ordering, and
      dependent nested-type substitution.
- [x] Pass native-Windows `test-cxx-class-template-specialization` for i686
      and AMD64 generation and AMD64 execution.
- [x] Order a fixed-arity partial specialization ahead of an otherwise
      equivalent trailing-pack specialization when the pack expands to zero;
      verify recursive `conjunction` substitution through an alias on both
      targets and against GCC with `test-cxx-dependent-pack-through-alias`.
- [x] Defer member-function-template body cloning until overload selection so
      body diagnostics are not mistaken for substitution failure; verify the
      matching structural-value-pack success and mismatch tests for i686,
      AMD64, and GCC.
- [x] Keep a declaration's parsed function-type `noexcept` state synchronized
      when it has no separate exception-specification expression; pass
      `test-cxx-noexcept-expression` for i686/AMD64 codegen, execution, and
      verified backend.
- [x] Re-run the full native-Windows MinGW GCC `test-cxx` aggregate after the
      pack-marker, zero-length-expansion ordering, and deferred body-diagnostic
      and noexcept-state corrections.

## RCC continuation (2026-10-24, local declaration attributes)

- [x] Preserve standard `[[deprecated]]` on block-scope variables, including
      every comma-separated declarator, and global- and namespace-scope
      block-scope function declarations plus deprecated function-template
      instantiation, ordinary deprecated classes, and deprecated class
      templates including explicit and partial specializations.
      `test-cxx-deprecated` checks fifteen RCC diagnostics on both
      target widths and checks GCC C++20 diagnostics for local, namespace,
      function-template, and class uses; the i686 COFF check passes and
      generated AMD64 code runs.
- [x] Preserve function deprecation across the tested redeclaration orders
      without retroactively changing the earlier call; GCC C++20 and RCC
      diagnostics are checked by the focused test.
- [x] Preserve class and primary class-template deprecation through type use;
      GCC C++20 diagnostics and the instantiated AMD64 code are checked.
- [x] Propagate class attributes through primary, explicit, and partial
      class-template selection; GCC C++20 diagnostics and both existing
      specialization regression targets pass.
- [ ] Other declaration/attribute ordering cases remain open; class-member
      behavior is covered by the same fixture.

## RCC continuation (2026-10-24, C++ constructibility trait)

- [x] Implement `__is_constructible(T, Args...)` for the modeled scalar,
      reference, explicit-constructor, deleted/inaccessible best-match, and
      C++20 aggregate cases. Dependent type packs are expanded after template
      substitution. `test-cxx-constructible-builtin` and instantiated
      `std::is_constructible` assertions pass for i686/AMD64 and GCC.
- [x] Advertise `__is_constructible` via `__has_builtin` and gate it through
      `test-preprocessor-has-builtin` for C/C++, both target widths, and the
      libcxx type-traits preprocessor/compile fixture.
- [x] Define RinOS `max_align_t` from the natural alignment of supported
      `long long` and `double` types, avoiding unsupported `long double` and
      field attributes. The direct libcxx type-traits fixture checks its
      supported scalar alignment on both targets and passes GCC C++20.
- [x] Extend `__is_constructible` to model implicit copy/move construction
      through base classes, fields, and array elements, including nontrivial
      and deleted member copies, non-const copy signatures, and move
      suppression by a user destructor. The focused builtin and direct libcxx
      fixtures pass for both targets and GCC C++20.
- [x] Include direct constructor-template candidates in `__is_constructible`.
      Cover forwarding-reference deduction/collapsing, `requires` filtering,
      default arguments, template partial ordering, non-template tie-breaking,
      private/deleted best matches, and interaction with implicit copy/move
      candidates. The focused builtin and direct libcxx fixtures pass for
      i686/AMD64 and GCC C++20 on Windows and WSL.
- [x] Include ordinary inherited constructors synthesized by `using Base::Base`
      in `__is_constructible` candidate selection. Verify inherited overload
      arity and reject excess arguments in the builtin and direct libcxx
      fixtures on Windows and WSL; the inherited-constructor runtime suite
      passes under WSL for both target architectures. Windows runtime
      integration remains unverified because the shell lacks `objdump`.
- [x] Preserve private and deleted ordinary base constructors in the
      type-trait-only inherited overload set so an inaccessible/deleted best
      match blocks a weaker public overload; do not treat base copy/move
      constructors as inherited. Both target-width fixtures match GCC C++20.
- [x] Ordinary runtime construction through bounded inherited constructor
      templates is covered for direct and linear multi-level public
      non-virtual chains. Final-layer direct bases may precede or follow the
      selected inherited base when each sibling has a safely lowerable public
      zero-argument constructor; i686/AMD64 runtime probes verify declaration
      order across repeated constructions.
- [ ] Remaining implicit copy/move deletion and access rules, more complex
      virtual-base inherited-constructor cases, and broader trait conformance
      remain open.

## RCC continuation (2026-10-24, assignability intrinsic)

- [x] Implement `__is_assignable(T, U)` by forming and semantically checking
      the corresponding assignment expression. Cover scalar conversions and
      cv/ref categories, ordinary and templated assignment operators,
      inaccessible/deleted best matches, implicit and explicitly defaulted
      copy/move assignment, subobject/array propagation, and union deletion
      rules. The focused builtin and direct libcxx `std::is_assignable`
      fixtures include dependent instantiation and match GCC C++20 on i686
      and AMD64.
- [x] Advertise `__is_assignable` through `__has_builtin`; the C/C++ feature
      query fixture passes on both target widths. The complete `make test-cxx`
      aggregate passes under WSL after the parser correction below.
- [x] Recognize in-class function-template declarations whose overloaded
      operator spelling contains `=`, so templated `operator=` reaches normal
      candidate materialization and overload resolution. The direct builtin
      and libcxx assignment-template cases pass on both target widths and
      GCC C++20.
- [x] Evaluate implicit union copy and move assignment independently against
      the corresponding variant-member operation. Cover scalar/array variants,
      deleted and non-trivial operations, trivial copy with non-trivial move,
      non-trivial copy with trivial move, and rvalue fallback to copy. GCC C++20
      and RCC agree in direct libcxx `std::is_assignable` tests for i686 and
      AMD64; the complete `test-cxx` aggregate reaches its successful final
      target.
- [ ] Constrained assignment-template edge cases, unusual cv/ref and union
      special-member rules, complete implicit copy/move access/deletion
      conformance, `is_nothrow_constructible`, and broader type-trait coverage
      remain open.
  - [x] Parse file-scope, namespace-scope, nested, and function-local C++ union
        definitions through the C++ class member parser. Model overlaid field
        layout and alignment, preserve union default-public access, support one
        default member initializer, and lower user-defined plus explicitly
        defaulted trivial copy assignment. Verify bytewise assignment when an
        unrelated `operator=(int)` overload is declared first, forward
        declarations, nested/local methods, GCC C++20 parity, i686/AMD64 object
        generation, and x64 host execution. Reject reference fields, virtual
        members, base classes, and multiple default member initializers on both
        targets.
  - [x] Parse namespace-qualified definitions for previously declared,
        named non-template class and union methods through one shared path.
        Reuse the in-class declaration for overload selection, access, mangling,
        and implicit-object ABI; cover static/private methods, inherited
        defaults, cv/ref qualifiers, `noexcept`, pointer/reference results, and
        overloads. GCC C++20 syntax and symbol names match, and i686/AMD64
        execution tests include assignment through returned references.
  - [x] Define previously declared non-template constructors outside their
        class, matching the declaration overload and preserving its default
        arguments while binding definition parameter names to constructor
        initialization. Cover namespace-qualified and namespace-relative
        owners, default/explicit/overloaded constructors, member initializer
        lists, constructor bodies, mismatched signatures, added defaults, and
        redefinitions. GCC C++20 syntax plus i686/AMD64 generation and runtime
        tests pass.
  - [x] Define namespace-qualified and namespace-relative non-template
        destructors outside their classes. Match the declared destructor,
        preserve its exception specification, register its body for ordinary
        method emission and recognized cleanup lowering, and verify automatic
        object lifetime execution plus mismatch/redefinition diagnostics on
        i686 and AMD64.
  - [x] Define and call a namespace-qualified non-template binary
        `operator+` outside its class using the existing member declaration for
        overload lookup, return type, mangling, and implicit-object ABI. Verify
        result execution plus return-type mismatch and redefinition diagnostics
        for i686 and AMD64.
  - [x] Define a namespace-qualified non-template `operator bool() const
        noexcept` outside its class. Reuse the declared conversion target and
        member ABI, perform the implicit conversion at a call site, and verify
        exception-specification, target mismatch, and redefinition diagnostics
        for i686 and AMD64.
  - [x] Define ref-qualified unary `operator*() &` and `operator*() &&`
        outside a named non-template class. Verify declaration matching,
        lvalue/rvalue-reference result categories, i686/AMD64 output, and AMD64
        execution; convert a class to a namespace-aliased function-pointer type
        through an out-of-class conversion function and execute the result.
        GCC C++20 `-Werror` execution agrees, and a definition with the wrong
        ref-qualifier is rejected for both targets.
  - [x] Normalize reference types to their referred-to class during binary
        operator lookup without replacing operand expressions, so ref-qualified
        member overload selection sees lvalue/xvalue category and a non-member
        overload is discovered when its right operand is an xvalue. Regressions
        cover `T& + int`, `T&& + int`, and `int + T&&`; GCC C++20 `-Werror`,
        i686/AMD64 generation, and AMD64 execution pass.
  - [x] Apply the same reference-aware lookup to C++20 relational rewriting
        through integer-returning `operator<=>`. A ref-qualified spaceship
        selects distinct lvalue/xvalue results for `T < int`; GCC C++20
        `-Werror`, i686/AMD64 generation, and AMD64 execution pass.
  - [x] Cover non-member integer-returning `operator<=>` lookup when the only
        class operand is an xvalue on the right (`int < T&&`). GCC C++20
        `-Werror`, i686/AMD64 generation, and AMD64 execution pass.
  - [ ] Extend out-of-class definitions to remaining overloaded operators and
        conversion-type-id declarators, member templates and explicit
        specializations, advanced nested/template owner qualification, and
        remaining ABI/lifetime coverage.
  - [x] Recognize the standard by-value copy-assignment form
        (`operator=(T)` and top-level-const `operator=(const T)`) as a copy
        special member. Keep it in ordinary assignment overload resolution,
        suppress the implicit copy/move assignment candidates it replaces, and
        require viable copy construction for the by-value argument. Direct
        libcxx assignability/trivial-copyability probes, an emitted rvalue
        assignment call, deleted-copy-constructor rejection, and rejection of
        an invalid defaulted by-value signature pass against GCC C++20 for
        i686 and AMD64.

## RCC continuation (2026-10-24, inherited constructor-template base order)

- [x] Initialize safely lowerable direct sibling bases declared before the
      selected inherited constructor-template base before invoking it, adjust
      the selected base `this` pointer, then initialize later sibling bases
      and derived members in declaration order. Repeated constructions verify
      prefix/suffix order and initialized values on i686 and AMD64; the focused
      inherited-constructor execution gate and full `make test-cxx` aggregate
      pass under WSL, and the GCC C++20 execution oracle passes.
- [x] Runtime inherited-constructor templates through a direct virtual base,
      a linear virtual-base chain, and a shared virtual-base diamond execute
      on i686/AMD64; virtual-base initializer/destructor events occur once.
- [x] Bounded inherited-constructor templates also execute through multiple
      inheritance at intermediate and final layers, with direct bases before
      and after the selected base; repeated i686/AMD64 runtime checks pass.
- [ ] Additional virtual-base constructor chains, broader constructor
      lifetime/exception cleanup, remaining
      implicit copy/move access/deletion rules, additional
      `is_nothrow_constructible` edge cases, and broader type-trait coverage
      remain open.

## RCC continuation (2026-10-24, inherited constructor-template traits)

- [x] Include constructor templates inherited through a public, non-virtual
      direct `using Base::Base` in `__is_constructible` overload selection.
      Deduction, constraint substitution, partial ordering, and deleted best
      matches are compared against GCC C++20 in the builtin and libcxx direct
      fixtures on i686 and AMD64.
- [x] Ordinary source construction and emitted derived-constructor bodies for
      bounded inherited constructor templates are covered by i686/AMD64
      runtime tests, including selected-base body execution and derived member
      initialization. Runtime support is separate from the `__is_constructible`
      candidate-probe coverage above.
- [x] Compare `__is_nothrow_constructible` through a direct virtual base, a
      linear virtual-base chain, and a shared virtual-base diamond with GCC
      C++20; the inherited-constructor runtime gate passes for i686/AMD64.
- [x] Check `__is_constructible` for an inherited constructor template through
      an intermediate class with multiple direct bases and through the final
      multi-base derived class; GCC C++20 and the i686/AMD64 target gate pass.
- [x] Check `__is_nothrow_constructible` through those intermediate and final
      multi-base layers when the selected template constructor, sibling-base
      constructors, and class members are all `noexcept`; GCC C++20 agrees and
      the focused target passes generation/execution on i686 and AMD64. Ensure
      the target creates its virtual-fixture output directories from a clean
      test-output tree.
- [ ] More complex virtual-base cases, remaining implicit copy/move
      deletion/access rules, and broader type-trait conformance remain open.

## RCC continuation (2026-10-24, no-throw constructibility)

- [x] Implement `__is_nothrow_constructible` using selected-constructor
      exception specifications, argument/default-argument conversions,
      aggregate/base/member initialization, defaulted copy construction, and
      temporary destruction. Direct builtin and libcxx
      `std::is_nothrow_constructible` cases match GCC C++20 on i686 and AMD64.
- [x] Advertise `__is_nothrow_constructible` through `__has_builtin`; C and
      C++ feature-query tests pass on both target widths.
- [x] Instantiate dependent libcxx `std::is_nothrow_constructible<T, Args...>`
      through nested `integral_constant` bases for both true and false results,
      including an argument pack; i686/AMD64 and GCC C++20 agree.
- [x] Fold unambiguous zero-argument calls in constructor default arguments
      and aggregate default member initializers through nested libcxx
      `integral_constant` bases. Both `noexcept` and potentially-throwing calls
      match GCC C++20 on i686 and AMD64; the complete `make OS=Linux test-cxx`
      suite passes under WSL.
- [x] Select scalar overloads in constructor default arguments and resolve
      explicitly qualified namespace calls in aggregate DMIs through nested
      libcxx trait bases. `int`/`long` overload selection and both exception
      outcomes match GCC C++20 on i686/AMD64.
- [x] Extend nothrow-trait call coverage to unqualified nested-namespace lookup
      and overload selection, class and hidden-friend ADL, class-argument
      conversion functions, and converting constructors. Throwing and
      `noexcept` outcomes agree with GCC C++20 on i686/AMD64; the complete
      `make OS=Linux test-cxx` suite passes under WSL.
- [x] Verify implicit and explicitly defaulted nothrow copy/move propagation
      for const/non-const lvalues and rvalues, deleted/private member copies,
      noexcept and throwing member copies, noexcept member moves, deleted-move
      copy fallback, and move suppression/deletion from user-declared
      destructor/assignment. Direct libcxx trait tests agree with GCC C++20 on
      i686/AMD64.
- [x] Preserve access checking while evaluating implicit-copy subobjects:
      protected base copies are accessible through public and private
      inheritance, friend classes can copy private bases, and protected member
      copies remain inaccessible through composition. `is_constructible` and
      `is_nothrow_constructible` agree with GCC C++20 on both target widths.
- [x] Preserve the corresponding access context for implicit and explicitly
      defaulted copy/move assignment subobjects: protected base assignment is
      usable by a derived assignment but inaccessible to an external query,
      private base assignment deletes that operation, protected member
      assignment remains inaccessible through composition, and a friend-derived
      class can assign its private base. Copy and move cases cover public and
      private inheritance. Direct `__is_assignable` and libcxx
      `std::is_assignable` fixtures agree with GCC C++20 on i686 and AMD64.
- [x] Resolve the declaring class from the lowered function declaration when
      a declaration-only member's source declaration does not yet carry owner
      metadata. Keep overload hiding and access checks correct for declaration-
      only copy assignments and defaulted protected/private base subobjects;
      `test-cxx-constructible-builtin` passes on i686/AMD64 and strict GCC
      C++20.
- [ ] Complete implicit copy/move deletion/access rules beyond this tested
      matrix and broader type-trait conformance.

## RCC continuation (2026-10-25, template-kind union access)

- [x] Restrict function-only `CxxTemplate` union reads to `TMPL_FUNCTION`
      while resolving dependent types and template packs. This prevents class
      template metadata from being interpreted as a function declaration.
      The `test-debug-info` corpus, including i686/AMD64 instantiation of
      `debug_info_cxx_template.cpp`, passes under WSL.

## RCC continuation (2026-10-25, reference sizeof and alignof)

- [x] Apply C++ reference `sizeof`/`alignof` semantics consistently in
      constant-expression parsing, semantic validation/evaluation, and both
      native code generators by measuring the referred-to type. Direct
      `sizeof(T&)`, `sizeof(const T&)`, and `alignof(T&)` assertions pass in
      `test-cxx-constructible-builtin` and
      `test-cxx-libcxx-type_traits-direct` for i686 and AMD64, with GCC C++20
      syntax checks.
- [x] Fix constrained forwarding assignment-template deduction for const
      lvalue arguments. The function-template instance cache now compares
      exact type arguments, including cv/ref qualifiers, instead of reusing a
      `double&` specialization for `const double&`. Ordinary assignment,
      `__is_assignable`, and libcxx `std::is_assignable` regressions pass for
      i686 and AMD64 against GCC C++20.
- [x] Compare function-template partial-ordering patterns after lvalue/rvalue
      reference adjustment when both candidates use the same reference
      category. This selects `const T&` over `T&` for const lvalues rather than
      reporting an ambiguous template call; `test-cxx-function-template-
      overloads` matches GCC C++20 on i686 and AMD64, and the complete
      `test-cxx` aggregate passes on Windows.

## RCC continuation (2026-10-25, C23 nullptr)

- [x] Implement C23 `nullptr` as a reserved token with the distinct scalar
      `nullptr_t` type, null-pointer comparisons/conversions, C23 conditional
      rules, initialization from integer null pointer constants, and variadic
      argument access. Keep `nullptr` an ordinary identifier in pre-C23 C and
      keep the feature C-only where specified.
- [x] Provide C23 `nullptr_t` in both RCC and bootstrap `<stddef.h>` headers.
      Verify size/alignment, `_Generic`, static and automatic initialization,
      pointer/boolean/conditional behavior, variadic access, invalid arithmetic
      and conditional diagnostics, and pre-C23 identifier compatibility.
      The focused target passes on i686 and AMD64; generated AMD64 programs
      execute successfully at `-O0` and `-O2`.
- [x] Make the class-template member-function test create its separate
      conversion-operator output directory when run with a clean `TEST_OUT`.
      The complete C++20 regression target passes under WSL with an isolated
      output tree.

## RCC continuation (2026-10-25, cv-qualified constrained assignment)

- [x] Verify constrained forwarding assignment-template participation for
      `volatile` and `const volatile` lvalue arguments, both with and without
      a deleted fallback overload. The direct libcxx `std::is_assignable`
      fixture compiles for i686 and AMD64 and matches GCC C++20.
- [ ] Other constrained assignment-template cv/ref cases, union special
      members, and complete copy/move deletion/access conformance remain open.

## RCC continuation (2026-10-25, C23 static_assert keyword)

- [x] Reserve `static_assert` as a C keyword only in C23 and retain its
      identifier behavior in C17; keep `_Static_assert` available in C17.
      Check the C17 identifier with GCC and RCC execution, and compile the
      single-argument C23 `static_assert` form in the C23 regression target.
- [ ] Broader C23 language and library conformance remains open.

## RCC continuation (2026-10-25, out-of-class class-template members)

- [x] Define ordinary member functions and member templates outside the
      primary class-template owner. Preserve the enclosing and member template
      parameter scopes across consecutive template heads, bind definitions to
      the primary class pattern, and reuse the declared methods for overload
      matching and code generation. Verify defaulted type/non-type member
      parameters, deduced and explicit calls, GCC C++20 syntax, i686/AMD64
      generation, PE architecture, optimized/unoptimized output, and AMD64
      runtime in `test-cxx-class-template-methods`.
- [x] Define a primary class-template owner's binary `operator+` outside the
      class. Verify dependent return and parameter types, GCC C++20 syntax,
      i686/AMD64 generation, PE architecture, optimized/unoptimized AMD64
      execution, and mismatched-return diagnostics for both target widths in
      `test-cxx-class-template-methods` and
      `test-cxx-member-methods-invalid`.
- [x] Route out-of-class class-template `operator()`, `operator[]`, and
      `operator->` through the shared qualified-operator scanner. Verify call,
      subscript, and arrow-member access at runtime, GCC C++20 syntax, both
      target widths, and mismatched-return diagnostics in
      `test-cxx-class-template-methods` and `test-cxx-member-methods-invalid`.
- [x] Support explicit specializations of overloaded member-function
      operators `operator+`, `operator[]`, `operator()`, and unary
      `operator!` on a fully specialized class-template owner, plus `operator+`
      on an ordinary class owner. Materialize member-template candidates during
      infix, subscript, call-operator, and unary-operator resolution so the
      specialization and competing overloads are selected correctly. The
      unary case uses a defaulted member-template parameter, which unary syntax
      cannot deduce from an argument. GCC C++20 syntax and exact Itanium symbols,
      i686/AMD64 assembly, AMD64 runtime, and RCC/GCC rejection of missing/
      surplus template heads pass in
      `test-cxx-class-template-member-function-explicit-specialization`.
- [ ] Explicit member specializations through dependent owner chains, remaining
      overloaded operator forms, and other ABI/lifetime cases remain open.

## RCC continuation (2026-10-25, member-template lookahead and glvalue category)

- [x] Keep out-of-class member-template declaration lookahead non-diagnostic:
      skip declaration attributes/specifiers and scan for the qualified
      member-id without invoking type parsing that performs semantic checks.
      Dependent `typename U::value_type` return types and the full C++20
      regression aggregate pass under WSL.
- [x] Preserve class-reference value category through conversion-function
      overload resolution. An xvalue now selects an `&&`-qualified conversion
      rather than an `&`-qualified overload; both-width invalid reference and
      runtime tests pass in `test-cxx-conversion-operator-posix`.
- [x] Preserve function designator addresses in both code generators instead
      of loading instruction bytes when dereferencing a function reference.
      The verified function-reference runtime regression passes.
- [x] Materialize the i686 host float-to-double reference value in the
      optimizer runtime test, avoiding a host-side decimal-constant mismatch.
      `test-optimize` passes on the native Windows target; the POSIX run passed
      its optimizer execution checks before the WSL shared-drive I/O failure.
- [ ] Explicit member specializations through dependent owner chains, other
      overloaded operator forms, and remaining ABI/lifetime cases remain open.

## RCC continuation (2026-10-25, class-template-owner member-template specialization)

- [x] Parse an explicit specialization of a named member-function template
      whose owner is a fully specialized class-template-id, including a
      concrete owner selected from a class-template partial specialization.
      Resolve the final owner before matching the declared member template,
      emit its specialization and unaffected primary-template instantiations,
      and compare exact Itanium symbols with GCC. Verify GCC C++20 syntax,
      i686/AMD64 assembly, AMD64 execution, and rejection of missing/surplus
      template heads in
      `test-cxx-class-template-member-function-explicit-specialization`.
- [x] Cover overloaded member-function-template specializations on a fully
      specialized class-template owner. Match the explicit pointer-signature
      specialization to the correct overload while preserving scalar and
      pointer primary instantiations; GCC symbols match, i686/AMD64 assembly,
      and AMD64 execution pass in
      `test-cxx-class-template-member-function-explicit-specialization`.
- [x] Define the primary member-template overloads out-of-class after explicit
      specializations on a fully specialized direct class-template owner.
      Refresh already-cached concrete owner declarations using the later
      primary body while preserving their member-template specialization cache;
      GCC C++20 syntax and symbols, i686/AMD64 assembly, and AMD64 link/run pass
      in `test-cxx-class-template-member-function-explicit-specialization`.
- [x] Resolve explicit member-function-template specializations through one-
      and two-level nested class-template owners under both class-template and
      ordinary outer classes. Parse the corresponding nested type-id chains,
      match the declaration on the final concrete owner, verify GCC's exact
      Itanium symbols, assemble both target widths, and execute the AMD64
      regression. Require exactly one explicit template head per specialized
      owner plus the member template; RCC and GCC reject missing and surplus
      heads without producing an object. The focused nested-owner test and
      complete native-Windows `test-cxx-nested-class-templates` aggregate pass.
- [x] Specialize overloaded member-function operators `operator+`,
      `operator[]`, and `operator()` on a fully specialized class-template
      owner, and `operator+` on an ordinary class owner. Materialize viable
      member-template overloads for infix, subscript, and call syntax; select
      the correct overloaded specialization while preserving pointer/scalar
      primary instantiations. Exact GCC Itanium symbols match; i686 and AMD64
      assembly, AMD64 execution, and missing/surplus template-head diagnostics
      pass in the focused class-template-owner target.
- [x] Keep the explicit member-specialization lookahead's two-token history
      current so a qualified member template-id is not classified as a free
      function-template specialization. The non-template, class-template, and
      nested-owner specialization gates pass on i686/AMD64 with GCC symbol
      comparisons and AMD64 execution.
- [x] Cover explicit member-template specialization through unary
      `operator~` syntax on a fully specialized class-template owner. The
      defaulted member-template parameter is selected through unary syntax;
      RCC and GCC emit the same Itanium symbol, both target widths assemble,
      and the AMD64 runtime distinguishes the specialization from the primary
      template in `test-cxx-class-template-member-function-explicit-specialization`.
- [x] Route source-written unary `&` on class objects through C++ overloaded
      operator resolution without changing synthesized address expressions.
      Make Itanium operator-name encoding arity-aware for unary/binary `+`,
      `-`, `*`, and `&`; the focused regression checks unary `operator&()` as
      `ad`, binary `operator&(T)` as `an`, exact GCC symbols, both target
      widths, and AMD64 runtime behavior. The focused specialization target
      plus `test-cxx-operator-overload`, `test-cxx-member-operator-forms`,
      and `test-cxx-nonmember-operator` pass natively on Windows; the latter
      two also verify GCC's unary `operator-` ABI name.
- [x] Preserve class-member scope in explicit out-of-class member-template
      specializations. Materialize the selected concrete member instance before
      semantic analysis so its implicit object and field lookup are available;
      the unary `operator&` specialization now refers to an unqualified member
      field and returns a distinct address from the primary template. GCC C++20
      syntax/symbol comparison, i686/AMD64 assembly, AMD64 execution, and the
      focused native-Windows specialization target pass.
- [x] Cover unary and binary member `operator*` function-template
      specializations through operator syntax. RCC and GCC emit matching
      Itanium symbols (`de` and `ml`), including unary specialization that
      omits the explicit template argument and selects its default. Both target
      widths assemble, AMD64 runtime checks distinguish specialized and
      primary implementations, and missing/excess template-head diagnostics
      pass in the focused target.
- [x] Resolve ref-qualified unary member-operator templates on class
      reference expressions. Normalize references only for owner lookup and
      retain the original receiver for `&`/`&&` overload ranking. Verify lvalue,
      xvalue, and prvalue selection, GCC's distinct `R`/`O` Itanium names,
      i686/AMD64 assembly, and AMD64 runtime in the focused target.
- [x] Preserve the selected nested class-template owner's member scope while
      analyzing out-of-class explicit member-function-template specializations.
      Regressions use unqualified fields from a nested and deeper owner, from a
      nested owner selected by an outer partial specialization, and from a
      nested member-class partial specialization. The focused target passes
      exact GCC symbol checks, i686/AMD64 assembly, AMD64 execution, and
      missing/excess owner-head diagnostics.
- [ ] Additional partially dependent owner patterns, other overloaded
      operator forms beyond those covered above, and remaining ABI/lifetime
      cases remain open.

## RCC continuation (2026-10-25, dependent out-of-class conversion target)

- [x] Parse an out-of-class conversion-function definition on a dependent
      class-template owner when its conversion type-id is `const T*`.
      Preserve the primary templated owner while its specialization remains
      dependent, then match and instantiate the conversion member. GCC C++20
      syntax validation and RCC i686/AMD64 generation plus `-O0`/`-O2` runtime
      checks pass in `test-cxx-class-template-methods`; the member-method and
      conversion-operator regression targets pass as well. Other cv/ref-
      qualified conversion targets and more deeply qualified dependent owners
      remain open.
- [x] Define `operator const T&()` out-of-class for a dependent class-template
      owner and bind it to a local `const int&`. Classify reference-valued
      operands by their referred-to type for built-in comparisons, including
      an explicit `static_cast<const int&>` comparison. GCC C++20 syntax and
      i686/AMD64 `-O0`/`-O2` runtime checks pass in
      `test-cxx-class-template-methods`; adjacent member and conversion
      operator suites pass. Other cv/ref target forms and deeper owner chains
      remain open.
- [x] Define `operator const T*() const&` and `operator T*() &&` out-of-class
      for a dependent class-template owner. Lvalues select the const-pointer
      conversion and rvalues the mutable-pointer conversion; dereference the
      latter within the temporary's full-expression. GCC and RCC emit matching
      Itanium conversion symbols on i686/AMD64 through
      `test-cxx-class-template-conversion-abi`; `-O0`/`-O2` execution passes in
      `test-cxx-class-template-methods`. Further cv/ref forms and deeper owner
      chains remain open.
- [x] Preserve the namespace of a class-template owner when registering its
      instantiated member functions. Compare the ref-qualified conversion
      operator's exact Itanium symbols against GCC for one- and two-level
      namespace-qualified owners on i686 and AMD64, and execute both cases at
      `-O0`/`-O2` in `test-cxx-class-template-methods`. Adjacent member-method
      and conversion-operator regression targets pass. Nested member class
      templates with their own template arguments remain open.
- [x] Resolve an out-of-class conversion definition through a dependent
      class-template owner to its ordinary nested class (`Owner<T>::Inner`).
      Mangle the nested class by its source name, without an empty template-id
      or internal instance tag; the exact conversion symbol matches GCC on
      i686/AMD64. `test-cxx-class-template-methods` passes syntax, ABI, and
      `-O0`/`-O2` runtime checks; member-method, conversion-operator, and
      class-template-specialization targets plus the full `test-cxx` suite
      pass. Nested member class templates with their own template arguments
      remain open.
- [x] Resolve an out-of-class conversion definition through a dependent
      class-template owner to a nested member class template with its own
      template argument (`Owner<T>::Inner<U>`). The exact ref-qualified
      conversion symbol `_ZNKR34NestedClassTemplateConversionOwnerIiE5InnerIlEcvPKlEv`
      matches GCC on i686/AMD64; syntax checks, both target assemblies,
      `-O0`/`-O2` runtime, adjacent conversion/member/specialization targets,
      and the full `test-cxx` suite pass. Deeper nested-template owner chains
      and remaining partially dependent owner forms stay open. Local commit;
      no push.
- [x] Resolve an out-of-class conversion definition through three dependent
      class-template owner levels (`Owner<T>::Inner<U>::Leaf<V>`). The exact
      ref-qualified Itanium conversion symbol
      `_ZNKR34NestedClassTemplateConversionOwnerIiE5InnerIlE4LeafIcEcvPKcEv`
      matches GCC on i686/AMD64; GCC syntax, RCC assembly, and `-O0`/`-O2`
      runtime pass in `test-cxx-class-template-methods`, and the ABI,
      member-method, conversion-operator, and class-template-specialization
      targets pass. Chains beyond this depth and partially dependent owner
      forms remain open. Local commit; no push.

## RCC continuation (2026-10-25, logical-not inline integer DWARF)

- [x] Preserve a runtime logical-not argument passed to a read-only inline
      integer parameter. Admit `EXPR_NOT` to the bounded expression path and
      emit `DW_OP_constu 0; DW_OP_eq` with target-width normalization. The
      regression verifies the call-specific parameter DIE, source frame slot,
      and exact opcode sequence for legacy and verified i686/AMD64 objects.
      Focused `test-debug-info-inline-expression` and full `test-debug-info`
      pass.
- [ ] Other unsupported runtime expression trees, mutable inline-parameter
      locations, register-only sources, and general optimized inline-variable
      tracking remain open.

## RCC continuation (2026-10-25, i686 wide logical-not inline DWARF)

- [x] Preserve a runtime `!long long` argument on i686 without relying on a
      64-bit generic DWARF stack entry: load both four-byte frame pieces, OR
      them, and compare the result with zero. AMD64 keeps its native-width
      load. Regressions check zero, low/high-word nonzero values, the sign bit,
      and `UINT64_MAX` in legacy and verified objects; focused and full
      `test-debug-info` pass.
- [x] Compose `!` over a computed frame-backed unsigned `long long` right-shift
      operand as low/high 32-bit words on i686, OR the words, and compare with
      zero. Verify `!(value >> 33)` on unsigned boundaries for legacy and
      verified i686/AMD64; focused and full `test-debug-info` pass.
- [x] Preserve `!global_u64` passed as an unsigned 64-bit inline argument on
      i686: load both global words with exactly two `ABS32U` relocations, OR,
      compare with zero, and emit the zero-extended result as two pieces.
      Verify zero, nonzero, low/high word boundaries, `UINT64_MAX`, and the
      initialized nonzero and zero globals in legacy and verified objects;
      focused and full `test-debug-info` pass.
- [ ] Other non-frame-backed 64-bit expression sources and broader optimized
      multi-piece variable locations remain open.

## RCC continuation (2026-10-25, inline logical-not of integer equality DWARF)

- [x] Preserve `!(frame_local == 0)` for a read-only inline integer parameter
      when both equality operands have the same integer type and fit the target
      DWARF stack width. Emit the comparison and outer logical-not with each
      target-width normalization represented explicitly. Focused and full
      `test-debug-info` pass for legacy and verified i686/AMD64 objects.
- [ ] Mixed-type conversions, i686 wide comparisons, and other unsupported
      runtime expression trees remain open.

## RCC continuation (2026-10-25, inline integer comparison DWARF)

- [x] Preserve same-type integer `!=`, `<`, `>`, `<=`, and `>=` expressions
      under logical-not when both operands fit the target DWARF stack width.
      Lower signed comparisons directly; map full-width unsigned ordering to
      signed DWARF comparisons by flipping each operand's sign bit. Exact
      expression checks cover signed operators and the unsigned boundary on
      i686/AMD64 for legacy and verified objects. The focused inline-expression
      test passes.
- [ ] Mixed-rank conversions beyond the tested `int`/`unsigned int` pair,
      i686 comparisons wider than its DWARF stack, and other unsupported
      runtime expression trees remain open.

## RCC continuation (2026-10-25, integer casts in inline DWARF expressions)

- [x] Compose an explicit integer cast inside a runtime comparison argument
      when source and destination fit the target DWARF stack. Normalize the
      source value first and then the cast destination; verify signed `int` to
      `unsigned int` followed by full-width unsigned ordering on i686/AMD64.
      Focused inline-expression and full `test-debug-info` pass for legacy and
      verified objects.
- [ ] Other implicit conversions outside this integer-comparison path, casts
      involving i686-wide operands, and unsupported expression trees remain
      open.

## RCC continuation (2026-10-25, implicit integer comparison conversions)

- [x] Apply integer promotions and the common signed/unsigned integer type
      when composing inline comparison operands. Verify both an explicit
      `int`-to-`unsigned int` cast and the implicit `int`/`unsigned int`
      conversion for a full-width unsigned ordering comparison, plus
      `(short)frame_local < 0` promotion to `int` and
      `(unsigned short)frame_local < 0x8000u` promotion/common conversion, on
      i686/AMD64. Focused inline-expression and full `test-debug-info` pass for
      legacy and verified objects.
- [x] Verify `long frame_local < 0x80000000u` using the target ABI's
      mixed-rank common type: unsigned 32-bit on i686 and signed 64-bit on
      AMD64. Exact DWARF checks include the constant's source-width
      normalization, frame load width, and the i686 unsigned ordering remap.
      Focused inline-expression and full `test-debug-info` pass for legacy and
      verified objects.
- [x] Promote non-scoped enum comparison operands through their underlying
      integer type before deriving the common type. Verify an enum with fixed
      `unsigned int` underlying type compared with `-1L`: i686 uses unsigned
      32-bit ordering while AMD64 uses signed 64-bit ordering. Evaluate boundary
      values in the emitted DWARF expression for legacy/verified objects.
      Focused inline-expression and full `test-debug-info` pass.
- [x] Apply usual arithmetic conversions to an `unsigned long` frame local
      compared with signed `-1`. Verify the target-width unsigned common type,
      literal and source normalization, and the boundary result at the maximum
      value on i686 and AMD64. Evaluate boundary values in legacy and verified
      DWARF expressions; focused and full `test-debug-info` pass.
- [x] Promote a fixed-underlying `unsigned char` enum to `int` before comparing
      it with `-1`. Verify the one-byte unsigned source normalization, signed
      four-byte comparison, and values from zero through `UCHAR_MAX` in legacy
      and verified i686/AMD64 DWARF expressions. Focused and full
      `test-debug-info` pass; GCC C++20 confirms the expression semantics.
- [x] Promote a fixed-underlying `signed char` enum to `int`, then apply the
      common unsigned type for comparison with `0x80000000u`. Verify sign
      extension followed by unsigned-int conversion and the `SCHAR_MIN`, `-1`,
      zero, and `SCHAR_MAX` boundary behavior in legacy/verified i686/AMD64
      DWARF expressions. Focused and full `test-debug-info` pass; GCC C++20
      confirms the expression semantics.
- [x] Promote both fixed-underlying operands before deriving the common type
      for different `unsigned char` and `signed char` enums. This fixes the
      incorrect unsigned-char common type for `unsigned_enum < signed_enum`;
      evaluate the result for every unsigned-char boundary region in legacy and
      verified i686/AMD64 DWARF. Focused and full `test-debug-info` pass; GCC
      C++20 confirms the enum-to-enum semantics.
- [x] Promote a fixed-underlying `unsigned short` enum to `int` before applying
      the mixed-rank conversion to `long`. Verify the emitted inline comparison
      and `!(enum_value < -1L)` at zero, both sides of the signed boundary, and
      `USHRT_MAX` in legacy and verified i686/AMD64 DWARF. Focused and full
      `test-debug-info` pass.
- [x] Compose frame-backed i686 64-bit signed and unsigned comparisons from
      high/low 32-bit words for all six operators (`==`, `!=`, `<`, `<=`, `>`,
      `>=`). Evaluate signed and unsigned boundary values in legacy and verified
      i686 DWARF; exercise the unchanged AMD64 path as well. Focused and full
      `test-debug-info` pass.
- [x] Convert 32-bit integer constants to the 64-bit common comparison type
      before composing i686 high/low words. Verify zero-extension of
      `0x80000000u` in `long long < unsigned int` and sign-extension of `-1`
      in `unsigned long long < int`, evaluating signed/unsigned 64-bit
      boundaries on legacy and verified i686/AMD64 objects. Focused and full
      `test-debug-info` pass; GCC C++20 accepts the fixture.
- [x] Extend frame-backed runtime `int` and `unsigned int` operands to the
      64-bit common comparison type. Verify signed extension for
      `long long < int` and zero extension for
      `unsigned long long < unsigned int`, evaluating 32-bit RHS boundaries
      against signed/unsigned 64-bit LHS boundaries on legacy and verified
      i686/AMD64 objects. Focused and full `test-debug-info` pass.
- [x] Preserve frame-backed `signed char`, `unsigned char`, `short`, and
      `unsigned short` operands through integer promotion and 64-bit comparison
      conversion. Evaluate each source's signed/unsigned boundary values against
      wide signed or unsigned locals on legacy and verified i686/AMD64 objects.
      Focused and full `test-debug-info` pass.
- [x] Promote a frame-backed C++ `bool` comparison operand to `int` before its
      common conversion with `long long`. Evaluate both representable boolean
      values against signed 64-bit boundaries on legacy and verified i686/AMD64
      objects. Focused and full `test-debug-info` pass.
- [x] Preserve frame-backed unscoped enum operands with signed-char,
      unsigned-char, signed-short, unsigned-short, signed-int, and unsigned-int
      underlying types through integer promotion and 64-bit comparison
      conversion. Evaluate signed and unsigned underlying-width boundaries
      against wide signed/unsigned values on legacy and verified i686/AMD64
      objects. Focused and full `test-debug-info` pass.
- [x] Lower frame-backed signed `long long` plus or minus `1LL` when used as
      an i686 wide-comparison operand. Carry and borrow across the low/high
      32-bit words are checked at boundary values; signed-overflow cases are
      excluded. Focused and full `test-debug-info` pass in legacy and verified
      i686/AMD64 configurations.
- [x] Verify explicit casts of frame-backed scoped enum RHS values with
      `unsigned char`, `signed int`, and `unsigned long long` underlying types
      in wide comparisons. Cover promotion, sign/zero extension, and split
      i686 64-bit frame loads at boundary values. Focused and full
      `test-debug-info` pass in legacy and verified i686/AMD64 configurations.
- [x] Compose comparison operands from the sum or difference of two
      frame-backed signed or unsigned 64-bit locals in i686 DWARF, including
      low-word carry/borrow into the high word. Evaluate signed boundary pairs
      while excluding overflow and unsigned pairs with defined modulo-2^64
      semantics in legacy and verified i686/AMD64. Focused and full
      `test-debug-info` pass.
- [x] Compose frame-backed i686 64-bit bitwise `&`, `|`, and `^` operands
      word by word and verify their signed comparison results for boundary
      pairs on legacy and verified i686/AMD64. Focused and full
      `test-debug-info` pass.
- [x] Lower a frame-backed signed `long long` bitwise-complement operand in
      i686 wide comparisons by applying DWARF `not` independently to both
      32-bit words. Verify boundary results on legacy and verified i686/AMD64;
      focused and full `test-debug-info` pass.
- [x] Recursively compose nested frame-backed i686 64-bit bitwise `&`, `|`,
      and `^` nodes word by word. Evaluate signed boundary pairs for a nested
      expression on legacy and verified i686/AMD64; focused and full
      `test-debug-info` pass.
- [x] Lower signed unary negation of a nested frame-backed i686 64-bit
      bitwise expression word by word, propagating a low-word borrow into the
      high word. Evaluate signed boundary pairs while excluding `LLONG_MIN`
      negation; focused and full `test-debug-info` pass on legacy and verified
      i686/AMD64.
- [x] Lower constant left shifts of a frame-backed unsigned `long long` in
      wide comparisons for counts 0, 31, 32, and 33, covering the i686 low/high
      word boundary and cross-word carry. Verify unsigned comparison boundary
      values on legacy and verified i686/AMD64 objects; focused and full
      `test-debug-info` pass.
- [x] Lower constant logical right shifts of a frame-backed unsigned
      `long long` in wide comparisons for counts 0, 1, 31, 32, 33, and 63.
      Verify unsigned boundary values and the i686 cross-word bit flow on
      legacy and verified i686/AMD64 objects; focused and full
      `test-debug-info` pass.
- [x] Lower constant arithmetic right shifts of a frame-backed signed
      `long long` in wide comparisons for counts 0, 1, 31, 32, 33, and 63.
      Verify negative, zero, and positive boundaries and i686 sign propagation
      on legacy and verified i686/AMD64 objects; focused and full
      `test-debug-info` pass.
- [x] Preserve bounded short-circuit integer `&&`/`||` and conditional (`?:`)
      expressions as inline DWARF values. Use `DW_OP_bra`/`DW_OP_skip` for
      address-width values and compose both selected 32-bit words and 64-bit
      truth tests for i686 `long long` comparison operands. Evaluate both
      short-circuit and conditional paths, including zero, negative, and
      positive frame values, on legacy and verified i686/AMD64 objects. The
      focused `test-debug-info-inline-expression` and full `test-debug-info`
      gates pass; GCC C++20 syntax checking passes.
- [x] Compose frame-backed i686 unsigned 64-bit multiplication operands from
      recursive addition/subtraction expressions before lowering their modulo-
      2^64 product with 16-bit partial products and carry reconstruction.
      Evaluate `(left + 0x100000001) * (right - 0x100000001)` across unsigned
      boundaries in legacy and verified i686 DWARF; retain the existing
      AMD64/direct-operand coverage. Focused `test-debug-info-inline-expression`
      and full `test-debug-info` gates pass.
- [x] Recursively lower a computed i686 unsigned 64-bit multiplication as an
      operand of another multiplication, composing nested low/high words and
      16-bit partial-product carries. Evaluate
      `((left + 0x100000001) * (right - 0x100000001)) *
      (left + 0x100000001)` over unsigned boundaries in legacy and verified
      DWARF. Focused `test-debug-info-inline-expression` and full
      `test-debug-info` gates pass.
- [x] Preserve an inline unsigned 64-bit multiply expression whose frame-backed
      operands are locals declared in a nested lexical block. Resolve both
      source variables through their location lists and frame offsets, then
      evaluate `(left + 0x100000001) * (right - 0x100000001)` at unsigned
      boundaries in legacy and verified i686 DWARF. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` gates pass.
- [x] Preserve root-level and nested comma expressions in computed inline
      integer arguments by describing the already-evaluated right operand
      without replaying the left operand. Evaluate a product of two
      comma-wrapped frame-backed unsigned 64-bit expressions at boundary values
      in legacy and verified i686/AMD64 DWARF, with the test evaluator modeling
      both stack widths. Focused `test-debug-info-inline-expression` and full
      `test-debug-info` gates pass.
- [x] Verify a direct non-frame-backed unsigned 64-bit inline argument sourced
      from a global object. Check the `DW_OP_addr` expression and exact
      address-width relocation for legacy and verified i686/AMD64 objects in
      `test-debug-info-inline-expression`.
- [x] Describe AMD64 unsigned 64-bit inline arguments formed by adding or
      subtracting an unsigned constant from a global object, including the
      commuted constant-plus-global form. Emit a relocated `DW_OP_addr`,
      `DW_OP_deref_size 8`, and the matching arithmetic/value operations;
      verify exact expressions and global relocations in legacy and verified
      `test-debug-info-inline-expression` objects, evaluating the expression
      against the relocated global's object data.
- [x] Describe AMD64 unsigned 64-bit inline arguments formed from a global
      object with an unsigned `&`, `|`, or `^` constant. Verify the relocated
      global load, exact DWARF opcode, and evaluated result from object data in
      legacy and verified `test-debug-info-inline-expression` objects.
- [x] Preserve AMD64 unsigned 64-bit inline arguments formed by multiplying a
      global object by `0x100000005`, even after optimization rewrites the
      multiply into a shift/add expression tree. Emit a relocation for each
      global leaf and evaluate the resulting DWARF expression against the
      initialized object data in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Preserve AMD64 unsigned 64-bit inline values formed by constant logical
      left and right shifts of a global object. Cover left-shift wrap at count
      33, logical right shift at count 3, and unsigned division by 8 after its
      power-of-two optimization. Emit the relocated global load, shift, and
      stack value; evaluate the results against initialized object data and
      verify exact relocations in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Preserve AMD64 unsigned 64-bit inline values formed by unsigned modulo
      by 8 after strength reduction to a bit mask. Verify the relocated global
      load, exact `DW_OP_and` expression, and result `7` against initialized
      object data in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Verify AMD64 global-backed unsigned logical right shift by 63 with the
      source high bit set, plus left shift by 1 with modulo-2^64 wrap. Evaluate
      `0xfedcba9876543217 >> 63 == 1` and
      `(0xfedcba9876543217 << 1) == 0xfdb97530eca8642e` from the emitted
      relocated DWARF expressions in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Lower AMD64 global-backed unsigned 64-bit bitwise complement in inline
      parameter values as a relocated load followed by `DW_OP_not`; verify the
      exact opcode and complement result in legacy and verified objects.
      Focused `test-debug-info-inline-expression` and full `test-debug-info`
      pass.
- [x] Lower unsigned AMD64 global-backed 64-bit unary negation as a relocated
      load followed by `DW_OP_neg`; verify modulo-2^64 boundary behavior and
      exact opcode in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Lower explicit unsigned-64 casts of global logical-not expressions as a
      relocated load, zero comparison, and 0/1 stack value; verify zero and
      nonzero global inputs in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Lower explicit unsigned-64 casts of AMD64 global equality and inequality
      comparisons with same-width constants. Verify exact `DW_OP_eq` and
      `DW_OP_ne` sequences and evaluate equal/non-equal results from initialized
      global data in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Lower AMD64 global-backed unsigned-64 `<`, `<=`, `>`, and `>=` inline
      comparisons with same-width constants. Bias both operands by flipping
      their sign bit before the signed DWARF comparison; verify exact opcode
      sequences and high-bit/`UINT64_MAX` boundaries in legacy and verified
      objects. Focused `test-debug-info-inline-expression` and full
      `test-debug-info` pass.
- [x] Preserve AMD64 global-backed unsigned-64 multiplication by 7 after
      optimization expands it to a multi-relocation expression tree. Evaluate
      the emitted expression against initialized data and verify the modulo-
      2^64 result in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Preserve AMD64 unsigned-64 inline expression trees that reference two
      distinct global objects. Verify one relocation to each exact symbol and
      evaluate the addition from both initialized `.data` values in legacy and
      verified objects. Focused `test-debug-info-inline-expression` and full
      `test-debug-info` pass.
- [x] Preserve AMD64 unsigned-64 multiplication of two distinct global
      objects. Verify one relocation per exact symbol, exactly one
      `DW_OP_mul`, and the modulo-2^64 product evaluated from both initialized
      `.data` values in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Preserve AMD64 unsigned-64 subtraction and XOR between distinct global
      objects. Verify one relocation per exact symbol, exactly one
      `DW_OP_minus`/`DW_OP_xor`, and the wraparound/high-bit results evaluated
      from both initialized `.data` values in legacy and verified objects.
      Focused `test-debug-info-inline-expression` and full `test-debug-info`
      pass.
- [x] Lower explicit unsigned-64 casts of AMD64 global-to-global `<` and `==`
      comparisons. Verify one relocation per exact symbol, sign-bit biasing
      for the unsigned less-than operation, exact comparison opcodes, and
      results evaluated from both initialized `.data` values in legacy and
      verified objects. Focused `test-debug-info-inline-expression` and full
      `test-debug-info` pass.
- [x] Lower explicit unsigned-64 casts of AMD64 global-to-global `<`, `<=`,
      `>`, `>=`, `==`, and `!=` comparisons. Cover reversed operand order and
      both high-bit orderings; verify one relocation per exact symbol, exact
      opcodes, unsigned sign-bit biasing, and results evaluated from both
      initialized `.data` values in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Preserve an AMD64 unsigned-64 inline multiplication of a global object
      by 9 after optimization rewrites it into a computed expression tree.
      Evaluate the expression from initialized global data in legacy and
      verified objects, allowing optimizer-dependent relocation-leaf counts
      while still validating each referenced symbol. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Preserve AMD64 unsigned-64 global-backed inline left and logical-right
      shifts with a nonconstant unsigned-64 global shift count. Verify both
      exact global relocations, `DW_OP_shl`/`DW_OP_shr`, and the evaluated
      results for a high-bit source shifted by four in legacy and verified
      objects. Focused `test-debug-info-inline-expression` and full
      `test-debug-info` pass.
- [x] Preserve the same AMD64 global-backed shifts when the dynamic count is a
      32-bit unsigned global. Emit `DW_OP_deref_size 4` for the count while
      retaining the 64-bit source load; verify both relocation symbols and
      high-bit shift results in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Lower an i686 unsigned-64 XOR of a global object and a 64-bit constant as
      separate relocated low/high 32-bit pieces. Verify both `ABS32U`
      relocations, piece opcodes, and the result evaluated from initialized
      global data in legacy and verified objects. Keep the AMD64 64-bit-stack
      expression emitters restricted to AMD64.
- [x] Extend the same i686 global-backed unsigned-64 lowering to bitwise AND
      and OR with a 64-bit constant. Verify the exact per-word opcodes, both
      address-width relocations, and results evaluated from object data in
      legacy and verified objects.
- [x] Lower i686 unsigned-64 bitwise complement of a global object as two
      relocated 32-bit pieces, each loaded and complemented on the i686 DWARF
      stack. Verify both `ABS32U` relocations and the result from initialized
      object data in legacy and verified objects.
- [x] Address the high word of every i686 global-backed bitwise piece with
      `DW_OP_plus_uconst 4`; verify this offset as part of the exact expression
      and recompute the full value from the initialized global object in both
      legacy and verified objects.
- [x] Lower i686 global-backed unsigned-64 constant shifts for counts 1..31
      using relocated low/high 32-bit pieces. Cover logical right shift by 3
      and left shift by 1; verify the exact cross-word expressions, all three
      `ABS32U` relocations, high-word `+4` offsets, and results evaluated from
      initialized global data in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Extend i686 global-backed unsigned-64 constant shifts through counts
      32..63. Verify both directions at count 32, left shift by 33, and logical
      right shift by 63; check exact one-relocation source-word expressions and
      the zero-valued pieces in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Preserve i686 global-backed unsigned-64 left and logical-right shifts
      by zero as two direct relocated word loads, without a shift opcode. Verify
      the exact low/high pieces, high-word `+4`, and initialized-data result in
      legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Lower i686 unsigned-64 negation of a global object as low/high 32-bit
      pieces with distinct `ABS32U` relocations for the low, high, and borrow
      source loads. Evaluate zero, low-word one, `UINT32_MAX`, a high-word-only
      value, `UINT64_MAX`, and the initialized global value in legacy and
      verified objects. Focused inline-expression and full `test-debug-info`
      gates pass.
- [x] Lower i686 unsigned-64 addition and subtraction between two global
      objects to low/high 32-bit pieces. Derive carry and borrow from unsigned
      low-word comparisons; verify the exact seven add relocations (4/3 per
      source), six subtract relocations (3/3), and wrap/carry/borrow boundary
      pairs plus initialized globals in legacy and verified objects. Focused
      inline-expression and full `test-debug-info` gates pass.
- [x] Lower i686 unsigned-64 AND, OR, and XOR between two global objects as
      independent low/high 32-bit operations. Verify exactly two `ABS32U`
      relocations per source, the high-word `+4` loads, and boundary results
      from initialized globals in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Lower i686 unsigned-64 casts of global-pair `==`, `!=`, `<`, `<=`, `>`,
      and `>=` comparisons. Bias unsigned 32-bit words for signed DWARF
      comparisons, combine high/low words, and zero-extend the boolean result.
      Verify 4 equality or 6 ordering relocations, initialized-global boundary
      pairs, and both legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Lower i686 unsigned-64 multiplication between two global objects as
      separate low/high 32-bit pieces. Reconstruct the high half of the low-word
      product from 16-bit partial products and carry, then include both
      cross-products. Verify nine `ABS32U` relocations per input and evaluate
      zero, carry-heavy, high-word, maximum-value, and initialized-global pairs
      in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Widen global `bool` and 8-, 16-, and 32-bit integers to i686 inline
      64-bit parameters for all source/destination signedness combinations.
      Use `DW_OP_deref_size`, zero-extend unsigned/bool sources, and sign-extend
      signed sources into both 32-bit pieces. Verify bool values and each
      integer width's signed boundaries, maxima, and negative values in legacy
      and verified objects. Focused `test-debug-info-inline-expression` and
      full `test-debug-info` pass.
- [x] Widen global scoped-enum values with fixed `unsigned char`, `signed
      short`, and `unsigned int` underlying types to both signed and unsigned
      i686 inline 64-bit parameters. Preserve the underlying storage width and
      sign/zero-extension; verify the exact source relocations and boundary
      results in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Extend global scoped-enum widening coverage to fixed `signed char`,
      `unsigned short`, and `signed int` underlying types, completing the
      signed/unsigned 8-, 16-, and 32-bit source matrix to both i686 inline
      64-bit destination signednesses. Verify source-width relocations and
      sign/zero extension on boundary values in legacy and verified objects.
      Focused `test-debug-info-inline-expression` and full `test-debug-info`
      pass.
- [x] Emit i686 global unsigned-64 division and remainder by nonzero 32-bit
      integer literals using relocated storage-word loads and the existing
      chunked unsigned-division algorithm. Verify divisors 7, 1073741825, and
      2147483649 across boundary dividend values, exact source relocations, and
      both legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [ ] Target-dependent `long` and 64-bit scoped-enum underlying types,
      narrowing/mixed-rank enum conversions, larger computed expression trees
      beyond bounded logical/conditional and
      multiplication cases, additional computed non-frame-backed i686 wide
      operands beyond global bool/8/16/32-bit widening and the completed global
      unary,
      bitwise, comparison, add/subtract, multiply, shift, and 32-bit-literal
      divide/remainder forms, other AMD64 computed-global
      multiplication shapes and divide/remainder, additional shift-count
      widths/conversions and shift forms, and mixed-rank conversion matrices
      remain open.

## RCC continuation (2026-10-26, signed i686 global 64-bit addition/subtraction)

- [x] Lower signed i686 global-to-global 64-bit addition and subtraction in
      read-only inline-parameter expressions using the same explicit low-word
      carry/high-word arithmetic and low-word borrow handling as the unsigned
      path. Require matching signed operand/result types; verify initialized
      globals, exact relocations, carry and borrow across the 32-bit boundary,
      and signed-range boundary pairs in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [ ] Volatile/reference/atomic operands and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-26, signed i686 global 64-bit bitwise operations)

- [x] Lower signed i686 global-to-global 64-bit AND, OR, and XOR by applying
      the exact same independent low/high 32-bit bitwise operations used for
      unsigned values. Require matching signed source/result types and verify
      negative initialized globals, exact relocations, and six bit-pattern
      pairs in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [ ] Volatile/reference/atomic operands and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-26, signed i686 global 64-bit multiplication)

- [x] Lower signed i686 global-to-global 64-bit multiplication in read-only
      inline-parameter expressions with the existing low/high 32-bit product
      reconstruction. Require matching signed source/result types; verify
      nine `ABS32U` relocations per input and signed boundary products plus
      initialized globals in legacy and verified objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [ ] Volatile/reference/atomic operands and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-26, signed i686 64-bit division and remainder)

- [x] Lower signed i686 64-bit `/` and `%` for matching signed identifier
      operands backed by globals, frame storage, or non-TLS function-local
      statics, and signed 64-bit literals.
      Convert operands to unsigned magnitudes, then restore quotient sign from
      operand-sign XOR and remainder sign from the dividend. Preserve the
      reloc-free frame/frame expression as well as global/global, global/frame,
      and frame/global operands; verify negative `-7` constants in legacy and
      verified objects. Cover signed boundaries while excluding division by
      zero and the undefined `INT64_MIN / -1` case. Require three exact
      `ABS32U` relocations per global operand for sign and magnitude loads;
      function-local static sources use source-file-scoped relocations and are
      checked at three relocations per static source. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass on
      native Windows; the static-local cases run for normal and verified i686
      objects.
- [ ] Wider computed signed operand trees beyond identifier operands, unary
      +/- and same-type signed 64-bit casts; TLS or
      volatile/reference/atomic operands; and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-26, signed i686 runtime divmod unary operands)

- [x] Extend signed i686 64-bit runtime `/` and `%` debug expressions to
      accept one unary minus around either identifier-backed operand. Invert
      only the operand's sign flag and reuse its exact two-word magnitude, so
      quotient sign and dividend-signed remainder semantics remain unchanged.
      Regressions cover global/global operands on C and C++ legacy/verified
      objects, plus frame/frame dividend negation and divisor negation on C
      legacy/verified objects. Boundary evaluation excludes negating `INT64_MIN`,
      division by zero, and `INT64_MIN / -1`. Both
      `make test-debug-info-inline-expression` and the full `make
      test-debug-info` gate pass under WSL2 Ubuntu-24.04/GCC.
- [x] Preserve identifier operands wrapped in an explicit same-type signed
      64-bit C cast or C++ `static_cast` for the same runtime division path.
      C and C++ tests verify cast-on-dividend division and cast-on-divisor
      remainder in legacy and verified i686 objects. C frame-local tests also
      cover both cast positions and operations, verifying the emitted frame
      expressions. Preserve type-identical unary `+` around signed 64-bit casts
      in C/C++ and unsigned 64-bit casts in C; these cases pass in legacy and
      verified i686 objects. Repeated unary-minus chains are reduced by
      parity and covered with even-negative dividend division and odd-negative
      divisor remainder in both languages. Apply the same signed dividend
      wrappers to literal-divisor division and remainder; C++ `+static_cast`
      global operands with `-7`, including a same-type cast on the literal
      divisor, pass in legacy and verified objects. Both focused and complete
      DWARF gates pass under WSL2 Ubuntu-24.04/GCC.
- [ ] Broader computed signed operand trees, width-changing casts,
      TLS or volatile/reference/atomic operands, and optimized
      multi-piece locations remain open.

## RCC continuation (2026-10-27, nested multiple-pointer structural NTTP)

- [x] Preserve Itanium substitution indices for a nested structural class NTTP
      containing three distinct non-null object-pointer leaves when an
      instantiated member function also takes repeated pointer parameters.
      Add `tests/cxx_structural_multiple_pointer_class_nttp.cpp`, verify
      structural-value type identity for equal pointer arrangements and
      distinct types for changed leaves, and compare exact GCC/RCC symbol sets.
- [x] Run `scripts/ci_nested_class_templates.sh`: GCC/RCC C++20 code generation
      and symbol parity pass for i686/AMD64, both AMD64 executables pass, and
      the complete POSIX nested-class regression gate passes.
- [x] Add the fixture to the native `test-cxx` aggregate for MinGW assembly
      and execution of RCC output plus the GCC C++20 host oracle.
- [ ] Other standard-valid structural class NTTP member shapes and broader
      mangling substitution contexts remain open.

## RCC continuation (2026-10-27, batched structural NTTP symbol checks)

- [x] Batch the structural-array target's exact GCC/RCC mangled-symbol checks
      by assembly file using `scripts/rcc_check_texts.py`, preserving every
      expected symbol including the target-specific x86 and host-x64 variants.
      Reduce separate search-process invocations from 82 to 13. The helper's
      present/missing cases and native-Windows
      `test-cxx-structural-array-nttp` pass; observed elapsed time fell from
      about 65 seconds to 22.5 seconds across the two runs.

## RCC continuation (2026-10-27, i686 static-local signed divmod DWARF)

- [x] Extend signed i686 64-bit runtime variable-divisor and literal-divisor
      `/` and `%` inline-parameter expressions to non-TLS function-local static
      identifiers. Resolve their local storage symbols, emit file-scoped
      `DW_OP_addr` relocations, and keep TLS, volatile, reference, and atomic
      operands rejected. Regressions cover both static operands for variable
      division/remainder and a static dividend with divisor `-7`; verify
      relocation identities and signed boundary-value evaluation in normal
      and verified i686 C++20 objects. Native Windows
      `make SHELL=cmd.exe test-debug-info-inline-expression` and full
      `make SHELL=cmd.exe test-debug-info` pass.
- [ ] Volatile/reference/atomic operands, wider computed signed expression
      trees, width-changing casts, and broader
      optimized multi-piece locations remain open.

## RCC continuation (2026-10-27, i686 unsigned static-local divmod DWARF)

- [x] Preserve unsigned i686 64-bit `/` and `%` for two non-TLS function-local
      static operands with a runtime divisor, and for a static dividend with
      high-bit literal divisor `0x8000000000000001`. Resolve storage symbols
      with source-file-scoped relocations and verify initialized values, the
      11-by-9 runtime-divisor matrix, constant-divisor boundary values, and
      exact relocation counts in normal and verified C++20 objects. Focused
      and full native-Windows `test-debug-info` gates pass.
- [ ] Volatile/reference/atomic operands, wider computed i686 64-bit
      division/remainder expressions, and broader
      optimized multi-piece locations remain open.

## RCC continuation (2026-10-28, constexpr constructor local references)

- [x] Bind initialized scalar constructor-body locals through read-only
      lvalue-reference locals, preserving aliasing when the original local is
      subsequently updated and through reference aliases. Reject reads through
      uninitialized targets and keep rvalue, aggregate, and subobject references
      out of this subset. Verify GCC C++20, constexpr/runtime values, i686/AMD64,
      and the full POSIX `test-cxx` suite.

## RCC continuation (2026-10-28, mutable constexpr constructor references)

- [x] Permit non-const lvalue-reference locals to alias initialized scalar
      constructor-body locals and propagate writes through chained aliases.
      Preserve const-reference reads, reject binding non-const references to
      const locals, and verify GCC C++20, constexpr/runtime, i686/AMD64, and
      full POSIX `test-cxx`.

## RCC continuation (2026-10-28, constexpr reference initializes local)

- [x] Allow a local reference to bind an indeterminate scalar local without
      reading it, then carry the target's initialized state through assignment
      via the reference or a chained alias. Reject any read before that write;
      verify GCC C++20, integer/floating constexpr and runtime values,
      i686/AMD64, and full POSIX `test-cxx`.

## RCC continuation (2026-10-28, signed i686 TLS divmod DWARF)

- [x] Emit signed i686 64-bit variable-divisor and negative-literal `/` and `%`
      inline-parameter expressions for global TLS operands using
      `DW_OP_const4s`, `DW_OP_form_tls_address`, and `RELOC_TLSOFF32S`. Verify
      expression values and relocation identities for normal and verified
      i686 C++20 objects; the full POSIX `test-debug-info` gate passes.
- [ ] Volatile/reference/atomic operands, wider computed signed expression
      trees, and broader optimized
      multi-piece locations remain open.

## RCC continuation (2026-10-28, unsigned i686 TLS divmod DWARF)

- [x] Extend unsigned i686 64-bit variable-divisor and constant-divisor `/`
      and `%` inline-parameter expressions to global TLS operands. Emit
      `DW_OP_const4s`, `DW_OP_form_tls_address`, and `RELOC_TLSOFF32S`; verify
      expression values and relocation identities in normal and verified
      i686 C++20 objects. The full POSIX `test-debug-info` gate passes.
- [ ] Volatile/reference/atomic operands, wider computed i686 64-bit
      division/remainder expressions, and broader
      optimized multi-piece locations remain open.

## RCC continuation (2026-10-28, i686 function-local TLS divmod DWARF)

- [x] Extend signed and unsigned i686 64-bit `/` and `%` inline-parameter
      expressions to block-scope `static thread_local` operands. Cover
      runtime-divisor TLS pairs and TLS dividends with signed `-7` or
      high-bit unsigned constant divisors; verify values and
      `RELOC_TLSOFF32S` identities in normal and verified C++20 objects.
      Focused `test-debug-info-inline-expression` passes; the full
      `test-debug-info` artifacts and verifier pass under WSL2 Ubuntu-24.04.
- [ ] Volatile/reference/atomic operands, wider computed i686 64-bit
      division/remainder expressions, and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-28, signed i686 TLS add/sub DWARF)

- [x] Extend signed i686 64-bit global-pair addition and subtraction inline
      expressions to TLS operands, using `DW_OP_const4s`,
      `DW_OP_form_tls_address`, and `RELOC_TLSOFF32S`. Verify the exact 4/3
      add and 3/3 subtract relocations, initialized TLS values, 32-bit carry,
      signed boundary pairs (including `INT64_MIN - (-1)`), and results in
      normal and verified C++20 objects. The focused
      `test-debug-info-inline-expression` gate passes under WSL2 Ubuntu-24.04.
- [ ] Volatile/reference/atomic operands and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-28, signed i686 TLS bitwise DWARF)

- [x] Extend signed i686 64-bit global-pair AND, OR, and XOR inline expressions
      to TLS operands, using `DW_OP_const4s`, `DW_OP_form_tls_address`, and
      `RELOC_TLSOFF32S`. Verify two relocations per operand, initialized TLS
      values, signed bit-pattern pairs, and results in normal and verified C++20
      objects. The focused `test-debug-info-inline-expression` gate passes under
      WSL2 Ubuntu-24.04.
- [ ] Volatile/reference/atomic operands and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-28, signed i686 TLS multiplication DWARF)

- [x] Extend signed i686 64-bit global-pair multiplication inline expressions
      to TLS operands. Emit TLS address formation for each low/high word load;
      verify all 18 `RELOC_TLSOFF32S` references, initialized TLS values,
      partial-product/carry cases, and signed boundary pairs in normal and
      verified C++20 objects. The focused
      `test-debug-info-inline-expression` gate passes under WSL2 Ubuntu-24.04.
- [ ] Volatile/reference/atomic operands and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-28, unsigned i686 TLS comparison DWARF)

- [x] Extend unsigned i686 64-bit global-pair `==`, `!=`, `<`, `<=`, `>`, and
      `>=` inline-parameter expressions to TLS operands. Emit
      `DW_OP_const4s`, `DW_OP_form_tls_address`, and `RELOC_TLSOFF32S`; verify
      both TLS symbol identities, the expected 2/3 relocations per operand,
      high-bit unsigned ordering, equality boundaries, and expression results
      in normal and verified C++20 objects. The focused
      `test-debug-info-inline-expression` gate passes under WSL2 Ubuntu-24.04.
- [ ] Volatile/reference/atomic operands and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-28, signed i686 TLS comparison DWARF)

- [x] Extend signed i686 64-bit global-pair `==`, `!=`, `<`, `<=`, `>`, and
      `>=` inline-parameter expressions to TLS operands. Compare the upper
      32-bit word as signed and the lower word as unsigned, and verify
      `TLSOFF32S` relocation identities, equality and signed-boundary pairs,
      and expression results in normal and verified C++20 objects. The focused
      `test-debug-info-inline-expression` gate passes under WSL2 Ubuntu-24.04.
- [ ] Volatile/reference/atomic operands and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-28, function-local static TLS comparisons)

- [x] Extend signed and unsigned i686 64-bit global-pair comparison
      expressions to block-scope `static thread_local` operands for all six
      relational/equality operators. Scope local TLS symbol relocations to the
      source module and verify their identities, initialized values, signed
      boundaries, and expression results in normal and verified C++20 objects.
      The focused `test-debug-info-inline-expression` gate passes under WSL2
      Ubuntu-24.04.
- [ ] Volatile/reference/atomic operands and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-28, function-local static TLS arithmetic)

- [x] Extend signed i686 64-bit global-pair addition, subtraction, AND, OR,
      XOR, and multiplication expressions to block-scope `static thread_local`
      operands. Scope local TLS relocation symbols to the source module and
      verify exact operand relocation counts, initialized values, carry/borrow,
      partial products, bit patterns, and expression results in normal and
      verified C++20 objects. The focused
      `test-debug-info-inline-expression` gate passes under WSL2 Ubuntu-24.04.
- [x] Extend the same six i686 arithmetic expressions to unsigned block-scope
      `static thread_local` operands. Verify module-scoped TLS relocations,
      exact relocation counts, initialized storage, wraparound, bitwise
      patterns, multiplication, and expression results in normal and verified
      C++20 objects. The focused `test-debug-info-inline-expression` gate
      passes under WSL2 Ubuntu-24.04.

## RCC continuation (2026-10-28, function-local non-TLS static arithmetic)

- [x] Verify signed and unsigned i686 64-bit block-scope `static` addition,
      subtraction, AND, OR, XOR, and multiplication expressions. Check
      `ABS32U` relocations (distinct from TLS relocations), module-scoped
      symbols, initialized values, wraparound/carry/borrow, bit patterns,
      multiplication, and expression results in normal and verified C++20
      objects. The focused `test-debug-info-inline-expression` gate passes
      under WSL2 Ubuntu-24.04.
- [ ] Volatile/reference/atomic operands and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-28, function-local non-TLS static comparisons)

- [x] Verify signed and unsigned i686 64-bit block-scope `static` `==`, `!=`,
      `<`, `<=`, `>`, and `>=` expressions. Check `ABS32U` operand relocations,
      module-scoped symbol identity, initialized values, unsigned high-bit and
      signed-boundary ordering, equality, and expression results in normal and
      verified C++20 objects. The focused
      `test-debug-info-inline-expression` gate passes under WSL2 Ubuntu-24.04.
- [ ] Volatile/reference/atomic operands and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-28, function-local static unary negation DWARF)

- [x] Extend i686 signed and unsigned 64-bit unary-negation inline-parameter
      expressions to block-scope `static` and `static thread_local` sources.
      Emit three module-scoped `ABS32U` relocations for non-TLS statics and
      three module-scoped `TLSOFF32S` relocations for TLS statics; verify the
      initialized bit patterns, low-word borrow behavior, and negated results
      in normal and verified C++20 objects. Both focused
      `test-debug-info-inline-expression` and full `test-debug-info` gates pass
      under WSL2 Ubuntu-24.04.
- [ ] Volatile/reference/atomic operands and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-28, function-local static bitwise complement DWARF)

- [x] Extend i686 signed and unsigned 64-bit bitwise-complement
      inline-parameter expressions to block-scope `static` and
      `static thread_local` sources. Emit two module-scoped `ABS32U`
      relocations for non-TLS statics and two module-scoped `TLSOFF32S`
      relocations for TLS statics; verify initialized bit patterns and
      complement results in normal and verified C++20 objects. The focused
      `test-debug-info-inline-expression` and full `test-debug-info` gates pass
      under WSL2 Ubuntu-24.04.
- [ ] Volatile/reference/atomic operands and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-28, signed static logical-not DWARF)

- [x] Extend signed and unsigned i686 64-bit logical-not inline-parameter
      expressions to block-scope `static` and `static thread_local` sources.
      Emit two module-scoped `ABS32U` relocations for non-TLS statics and two
      module-scoped `TLSOFF32S` relocations for TLS statics; verify the
      nonzero data and zero-initialized TLS sources, truth-value results, and
      normal/verified C++20 objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` gates pass
      under WSL2 Ubuntu-24.04.
- [ ] Volatile/reference/atomic operands and broader optimized multi-piece
      locations remain open.

## RCC continuation (2026-10-28, unsigned i686 static-local dynamic-shift DWARF)

- [x] Extend unsigned i686 64-bit inline-parameter left/right shift
      expressions to read non-TLS block-scope `static` source and shift-count
      objects. Preserve module-scoped `ABS32U` relocations, verify both static
      symbol identities and initialized values, and evaluate boundary shift
      counts through the emitted expression in legacy and verified i686 C++20
      objects. The focused `test-debug-info-inline-expression` gate passed for
      both paths.
- [ ] Atomic operands, wider computed signed trees, and
      broader optimized multi-piece locations remain open.

## RCC continuation (2026-10-28, i686 TLS dynamic-shift DWARF)

- [x] Extend i686 64-bit inline-parameter left/right shift expressions to
      block-scope `static thread_local` source and shift-count objects. Emit
      module-scoped `TLSOFF32S` relocations, verify both TLS symbol identities
      and template values, and evaluate boundary counts through the emitted
      expression in legacy and verified i686 C++20 objects. The focused
      `test-debug-info-inline-expression` gate passed for both paths.

## RCC continuation (2026-10-28, signed i686 static dynamic-shift DWARF)

- [x] Extend signed i686 64-bit inline-parameter shifts to block-scope `static`
      and `static thread_local` source objects with static 64-bit counts.
      Verify module-scoped `ABS32U`/`TLSOFF32S` relocations, initial values,
      defined left-shift count boundaries and negative arithmetic-right-shift
      results in legacy and verified i686 C++20 objects. The focused
      `test-debug-info-inline-expression` and full `test-debug-info` gates
      passed for both paths.
- [ ] Atomic operands, wider computed signed trees, and
      broader optimized multi-piece locations remain open.

## RCC continuation (2026-10-28, namespace-scope TLS dynamic-shift DWARF)

- [x] Verify i686 signed and unsigned dynamic left/right shifts over
      namespace-scope `thread_local` source/count objects. Check global TLS
      symbol identities (without local-symbol scoping), `TLSOFF32S` relocation
      types, initial template values, defined left-shift boundaries, and
      negative arithmetic-right-shift results in legacy and verified C++20
      objects. Focused `test-debug-info-inline-expression` and full
      `test-debug-info` gates passed.
- [ ] Atomic operands, wider computed signed trees, and
      broader optimized multi-piece locations remain open.

## RCC continuation (2026-10-28, volatile i686 dynamic-shift DWARF)

- [x] Emit i686 unsigned 64-bit dynamic left/right-shift inline-parameter
      expressions for volatile namespace-scope source and count objects. Keep
      `ABS32U` relocation identities and per-operand reference counts exact.
      Permit inlining of a volatile-read tree only for one scalar parameter
      used exactly once, preserving exactly-once evaluation. Verify initial
      storage, relocation targets, and defined shift-count boundaries in
      legacy and verified C++20 objects;
      focused `test-debug-info-inline-expression` and full `test-debug-info`
      pass under WSL2 Ubuntu-24.04.

## RCC continuation (2026-10-28, volatile namespace-scope TLS shift DWARF)

- [x] Extend volatile unsigned i686 64-bit dynamic left/right-shift inline
      expressions to namespace-scope `thread_local` source and count objects.
      Verify `TLSOFF32S` relocation identities and per-operand reference
      counts, initial storage, and defined shift-count boundaries in legacy and
      verified C++20 objects. Focused `test-debug-info-inline-expression` and
      full `test-debug-info` gates passed under WSL2 Ubuntu-24.04.

## RCC continuation (2026-10-28, volatile block-scope static shift DWARF)

- [x] Extend volatile unsigned i686 64-bit dynamic left/right-shift inline
      expressions to block-scope non-TLS `static` source and count objects.
      Verify module-scoped `ABS32U` relocation identities and per-operand
      reference counts, initial storage, and defined shift-count boundaries in
      legacy and verified C++20 objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` gates
      passed under WSL2 Ubuntu-24.04.

## RCC continuation (2026-10-28, volatile block-scope static TLS shift DWARF)

- [x] Extend volatile unsigned i686 64-bit dynamic left/right-shift inline
      expressions to block-scope `static thread_local` source and count
      objects. Verify module-scoped `TLSOFF32S` relocation identities and
      per-operand reference counts, initial storage, and defined shift-count
      boundaries in legacy and verified C++20 objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` gates
      passed under WSL2 Ubuntu-24.04.

## RCC continuation (2026-10-28, signed volatile i686 dynamic-shift DWARF)

- [x] Verify signed i686 64-bit dynamic left/right-shift inline expressions
      for volatile namespace-scope globals, namespace-scope TLS, block-scope
      `static`, and block-scope `static thread_local` source/count objects.
      Check exact `ABS32U`/`TLSOFF32S` symbol identities and relocation counts
      (6 source/8 count for left shift; 7 source/8 count for arithmetic right
      shift), initialized storage, defined left-shift boundaries, and negative
      arithmetic-right-shift values in legacy and verified C++20 objects.
      Focused `test-debug-info-inline-expression` and full `test-debug-info`
      gates passed under WSL2 Ubuntu-24.04.
- [ ] Atomic operands, reference chains/indirect sources, wider computed
      signed trees, and broader optimized multi-piece locations remain open.

## RCC continuation (2026-10-28, volatile lvalue-reference inline DWARF)

- [x] Inline read-only scalar lvalue-reference calls when the argument is a
      direct stable identifier. Preserve volatile-read evaluation constraints
      and emit the referenced global/TLS/automatic object's actual location
      (`ABS32U`/`ABS64`, `TLSOFF32S`, or frame-base expression). Verify the
      symbol and expression in i686/x64 legacy and verified C++20 objects with
      `test-debug-info-inline-expression`.
- [ ] Atomic operands, reference chains/indirect sources, i686 computed
      signed trees, and broader optimized multi-piece locations remain open.

## RCC continuation (2026-10-28, signed x64 computed-global DWARF)

- [x] Emit signed x64 inline-parameter locations for computed global integer
      expressions, including nested add/multiply trees and arithmetic
      right-shift. Keep signed comparisons and unsigned-only conversions on
      their existing semantics. Verify global relocation identities and
      evaluated values for signed add/subtract/multiply/bitwise operations,
      nested expressions, and negative arithmetic shifts in C++20 legacy and
      verified objects; `test-debug-info-inline-expression` and the full
      `test-debug-info` gate pass under WSL2 Ubuntu-24.04.

## RCC continuation (2026-10-28, signed x64 TLS computed DWARF)

- [x] Emit signed x64 computed inline-parameter expressions over stable
      namespace-scope TLS integers with `TLSOFF32S` relocations and
      `DW_OP_form_tls_address`, keeping TLS loads distinct from `ABS64` global
      loads. Verify the relocated symbols and evaluated TLS-template values
      in C++20 legacy and verified objects; focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass
      under WSL2 Ubuntu-24.04.

## RCC continuation (2026-10-28, signed volatile x64 global shift DWARF)

- [x] Emit signed x64 computed inline-parameter expressions that read
      volatile namespace-scope source/count globals. Verify exactly one
      `ABS64` relocation per operand and the negative arithmetic-right-shift
      result in legacy and verified C++20 objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` gates pass
      with `make SHELL=cmd.exe`.

## RCC continuation (2026-10-28, signed volatile x64 TLS shift DWARF)

- [x] Verify signed x64 computed inline-parameter expressions reading
      volatile namespace-scope TLS source/count objects. Check exactly one
      `TLSOFF32S` relocation per operand, `DW_OP_form_tls_address`, and the
      negative arithmetic-right-shift result in legacy and verified C++20
      objects. Focused `test-debug-info-inline-expression` and full
      `test-debug-info` gates pass with `make SHELL=cmd.exe`.

## RCC continuation (2026-10-28, volatile signed x64 left-shift DWARF)

- [x] Verify volatile signed x64 dynamic left shifts over namespace-scope
      globals and TLS objects, with one `ABS64`/`TLSOFF32S` relocation for
      each source/count operand and the `DW_OP_shl` value. Cover legacy and
      verified C++20 objects; focused `test-debug-info-inline-expression`
      and full `test-debug-info` pass with `make SHELL=cmd.exe`.

## RCC continuation (2026-10-28, frame-indirect scalar parameter DWARF)

- [x] Emit inline scalar parameter values for the bounded `*identifier` case
      where the identifier is a stable frame-resident pointer to a nonvolatile,
      non-atomic integer. Load the pointer from its frame slot and then load the
      pointee with target-sized `DW_OP_deref_size` operations; cover i686/x64
      legacy and verified C and C++20 backends. Interpret the emitted DWARF
      operations against signed boundary values. Focused
      `test-debug-info-inline-expression` passed with `make SHELL=cmd.exe`; the
      full debug-info verifier also passed after the remaining Make recipe
      steps were run directly because `cmd.exe` misparsed its multiline output
      redirection.
- [x] Extend the same frame-local expression to `**identifier` pointer chains
      by loading one address-sized pointer layer per dereference before the
      scalar load. Verify C and C++20 i686/x64 legacy and verified objects by
      interpreting the generated expression against a chained frame-memory
      model; focused `test-debug-info-inline-expression` and the full
      debug-info verifier passed on refreshed C outputs. The full Make
      target's `cmd.exe` redirection issue is documented above.
- [x] Verify the same expression across `***identifier` chains. Add C and
      C++20 fixtures with three pointer loads and a frame-memory model for both
      intermediate slots; cover i686/x64 legacy and verified objects with
      signed boundary values. `test-debug-info-inline-expression` passes.
- [x] Verify global/TLS/static pointer sources, volatile or atomic pointer and
      pointee, i686 wider-than-address-size pointees, and C++20 data-member-
      pointer accesses do not produce the frame-local indirect-value
      expression. The verifier permits omission of the inline-call DIE for
      volatile and atomic cases that must preserve the original read
      semantics. Focused `test-debug-info-inline-expression` passed.

## RCC continuation (2026-10-28, SDK inline assembly and C null constants)

- [x] Encode AMD64 `movq %fs:disp32, %r64` and `%gs` absolute segmented loads,
      including SIB/no-base addressing and signed 32-bit offsets. Verify exact
      FS/R10 and GS/RAX instruction bytes; compile the actual C11 threads and
      `ucontext.h` SDK consumers for i686 and AMD64. These checks are included
      in `test-inline-asm-encoding` and the C17 regression aggregate.
- [x] Stop defining `__STDC_NO_THREADS__` while the RinOS SDK provides
      `<threads.h>`; compile a consumer exercising mutex, condition, TSS,
      once, sleep, and thread APIs for both target architectures.
- [x] Recognize an integer constant expression of zero cast to `void*` as a
      C null pointer constant for object- and function-pointer initialization
      and assignment. Compare C17 acceptance with GCC and keep C++ implicit
      `void*` conversion rejected on both target architectures.
- [x] Declare the Windows exception-runtime prerequisite for both dependent
      nested-alias execution tests; `test-cxx-class-template-dependent-nested-type`
      now completes on native Windows.

## RCC continuation (2026-10-28, compute local-size shader builtins)

- [x] Expose `local_size_x/y/z` as compute-stage builtins and verify their
      RSH1 builtin IDs and declared `@workgroup(2, 3, 1)` dimensions from the
      Aquamarine fixture. The check is part of `test-aqc` and therefore the
      production test gate; native Windows shell redirection is covered.
- [x] Use native absolute compiler paths for redirected Makefile test recipes
      on Windows; run the C17, debug-info, AQC, verified floating-point, and
      affected C++ conversion/member-pointer/operator-arrow/diagnostic suites.
- [x] Keep the full member-function-pointer/TLS translation unit on the RinOS
      `.ro` path for i686 and AMD64, while limiting the MinGW host-execution
      adapter to the compatible non-TLS cases; verify `test-tls` and
      `test-cxx-static-member-tls` for both target architectures.

## RCC continuation (2026-10-28, nested frame-backed signed i686 DWARF)

- [x] Compose i686 signed 64-bit inline-parameter values for frame-backed
      nested add/sub expressions from recursively computed low/high words,
      preserving low-word carry and borrow. Verify both `left + (right << 1)`
      and `left - (right << 1)`, plus nested constant multiplication in
      `left + right * 2`, over signed boundaries by interpreting the emitted
      `DW_OP_piece` expression in both legacy and verified C++20 objects. Focused
      `test-debug-info-inline-expression` and full `test-debug-info` pass.
- [x] Compose i686 signed 64-bit global and TLS add/sub operands nested as
      `left ± (right * 2)` into low/high `DW_OP_piece` values, including the
      cross-word high bit from doubling; also verify the TLS `left + (right <<
      1)` form. Interpret signed-boundary cases in both legacy and verified
      C++20 objects with global and TLS relocations. Focused
      `test-debug-info-inline-expression` passes.
- [x] Extend the same i686 signed nested add/sub expressions to volatile
      namespace-scope globals and TLS. Preserve both words and exact global/TLS
      relocations; interpret signed-boundary results in legacy and verified
      C++20 objects; focused `test-debug-info-inline-expression` and full
      `test-debug-info` pass.
- [ ] Atomic reads, volatile operands outside this bounded global/TLS form,
      more general computed division/remainder operands, and optimized
      multi-piece locations remain open.

## RCC continuation (2026-10-28, C++20 relational rewrite candidate ranking)

- [x] Compare direct `operator<`, `operator>`, `operator<=`, and `operator>=`
      candidates with normal and reversed `operator<=>` rewritten candidates
      before lowering. Prefer a better conversion sequence, prefer the direct
      spelling on an equal sequence, and lower reversed candidates with the
      operands in the correct comparison order. Also allow class-valued
      spaceship results to use their own comparison-with-zero operators.
      Regression coverage checks better-spaceship and direct-tie selection,
      all four reversed relational spellings, and a class-valued comparison
      result; `test-cxx-spaceship` passes for i686/AMD64 generation and x64
      host execution. GCC C++20 `-pedantic-errors` agrees on a focused oracle.
- [x] Materialize viable member-function templates for both the requested
      relational spelling and `operator<=>` before candidate ranking. Cover a
      spaceship-only member template and equal-rank direct-template preference
      in `test-cxx-spaceship`. Restrict ranking to specializations materialized
      for the current operands, and reject a stale specialization after a later
      constraint mismatch. The focused target and overload/operator/
      defaulted-equality regressions pass, and GCC C++20 agrees.
- [ ] Complete the full standard candidate set and ranking interactions,
      including remaining built-in conversion candidates, function-template
      ordering, and hidden-friend/ADL corner cases; retain the broader C++20
      overload conformance item as open.
- [x] Apply C++20 derived-class visibility and using-declaration hiding to
      ordinary member overload candidate collection. A direct declaration
      hides base overloads without `using`; with `using`, only the selected
      base overload set is added and a same-signature derived declaration
      hides its base counterpart. `test-cxx-function-template-overloads`
      passes GCC C++20 `-Wall -Wextra -Werror`, i686/AMD64 code generation, and
      generated i686/AMD64 runtime execution.
- [x] Apply using-declaration hiding to inherited member-function-template
      candidates using declaration patterns before specialization. A derived
      `choose(T)` hides the matching base template while a distinct base
      `choose(T*)` and a same-pattern base template with a different
      constraint remain eligible. GCC C++20 and
      `test-cxx-function-template-overloads` pass for i686/AMD64 generation and
      execution.

## RCC continuation (2026-10-28, hidden-friend ADL candidate identity)

- [x] Deduplicate an ordinary-lookup overload-set copy against the original
      namespace declaration when ADL merges the same function. Preserve
      hidden-friend association filtering while avoiding duplicate overloads
      that made unrelated C++20 spaceship expressions ambiguous. Cover a
      reversed hidden-friend `operator<=>` alongside unrelated global
      spaceship comparisons in `test-cxx-spaceship`, including hidden friends
      in global, named, and inline namespaces; focused spaceship, overload,
      operator-overload, and defaulted-equality tests pass. A GCC C++20
      `-pedantic-errors` runtime oracle confirms the reversed rewrite in named
      and inline namespaces.
- [x] Materialize a non-template hidden friend declared in a class-template
      pattern for each class specialization, retain that specialization as the
      friend-access owner, and expose the declaration through associated-class
      ADL. Cover private-member access and distinct `int`/`long` specializations
      of a hidden-friend `operator<=>` in `test-cxx-spaceship`; the x86/x64
      assembly, x64 host runtime, class-template method/friend regressions, and
      an isolated GCC C++20 `-pedantic-errors` oracle pass.
- [x] Materialize viable operator function-template candidates from ordinary
      lookup and ADL before relational rewrite ranking, apply function-template
      partial ordering, compare conversion sequences against non-template
      candidates, and bind the selected specialization directly for emission.
      Cover a hidden-friend `operator<=>` template with private access and an
      equal-conversion non-template overload (`int` prefers the non-template,
      `long` the template), the `T*`-over-`T` partial-ordering case, and a
      namespace-scope operator template outside the operand's associated
      namespace. The x86/AMD64 spaceship target, host runtime,
      overload/operator/defaulted-equality regressions, and GCC C++20
      `-pedantic-errors` runtime oracle pass.
- [x] Preserve hidden friend function templates declared in class templates
      on each concrete class specialization. Substitute the enclosing class
      arguments into the friend signature and body while leaving the friend
      template's own parameters deducible; associate the materialized template
      with that specialization for ADL and private access. Cover `int` and
      `long` class specializations with independently deduced friend-template
      argument types in `test-cxx-spaceship`; x86/x64 compilation, x64 runtime,
      related overload/class-template regressions, and a GCC C++20
      `-pedantic-errors` runtime oracle pass.
- [x] Store the canonical namespace template in a class's hidden-friend ADL
      list when a friend declaration redeclares an existing function template.
      This prevents the same entity from entering overload resolution twice;
      friend requires-expression and mismatch diagnostics pass for i686/AMD64,
      with x64 execution and GCC C++20 syntax checks.
- [x] Merge equivalent ordinary function-template declarations with their
      later definitions, including a template default added on the definition,
      so calls see one specialization rather than duplicate candidates. Extend
      redeclaration type identity to distinguish nested function-pointer
      `noexcept` and member-function-pointer ref-qualifiers. Runtime, both-target
      function-template compilation, overload/ref-qualifier regression, and
      GCC C++20 `-pedantic-errors` checks pass.
- [ ] Continue auditing ADL candidate identity and hidden-friend lookup across
      function-template redeclarations, specializations, and the remaining
      C++20 candidate-set and overload-conformance cases.
- [x] In call-expression ADL, recursively collect associated classes from a
      function-pointer type's return and parameter types, including nested
      pointer layers, member-function-pointer owners/signatures, and base
      classes. Regress hidden friends found only from callback return types,
      member-function-pointer return types, or a callback parameter's derived
      class; strict GCC C++20 and RCC i686/AMD64 generation plus AMD64 runtime
      pass in the dedicated target and adjacent ADL/friend/two-phase/spaceship
      gates.
- [x] Include built-in arithmetic candidates in C++20 relational and equality
      resolution for class operands with implicit conversion functions. Rank
      them together with direct and rewritten candidates, then materialize the
      selected conversions. Cover class/class and class/scalar expressions in
      both operand orders with `int`, `long`, and `double`, and reject ambiguous
      `int`/`long` conversion sequences. `test-cxx-spaceship` passes i686/AMD64
      generation and x64 execution; GCC 13.2 strict positive/negative oracles
      agree.
- [x] Parse namespace-scope explicit specializations of free function
      templates and attach each specialization to the selected primary by its
      explicit template-id and declared function type. Cover qualified
      namespace names, overload selection, non-type and defaulted template
      arguments, a declaration followed by its definition, dependent
      `noexcept` matching, target ABI symbol parity, and x64 runtime behavior.
      Diagnose specialization after
      instantiation, duplicate definitions, forbidden specialization default
      arguments, and mismatched function signatures; RCC checks both targets
      and GCC C++20 `-pedantic-errors` oracles pass.
- [x] Parse explicit template arguments per candidate when overloaded free
      function templates use different type and non-type parameter kinds.
      Select the specialization by both that candidate-specific argument list
      and the declared function type; verify GCC-matching symbols and runtime
      results for explicit type and integral arguments on i686/AMD64.
- [x] Parse explicit arguments per candidate for overloaded free-function
      template calls whose candidates use different type and non-type
      parameter kinds. Preserve those candidate-specific arguments through
      dependent calls in enclosing function templates; test immediate and
      dependent type/non-type specializations with i686/AMD64 output, x64
      runtime, and GCC C++20 syntax/symbol comparisons.
- [x] Match explicit arguments per candidate when an overload set combines a
      pure type pack, a fixed non-type parameter followed by a type pack, and
      a pure non-type pack. Verify both selected results at runtime, GCC/RCC
      Itanium symbols, and a no-match pack-shape diagnostic on i686/AMD64.
- [x] Match template-template and non-type explicit arguments per overload
      candidate, including a dependent call preserved inside an enclosing
      function template. Verify runtime selection and GCC/RCC Itanium symbol
      parity on i686/AMD64.
- [x] Match explicit arguments per candidate when one overload uses a
      template-template parameter pack and another uses a type pack; cover
      one- and two-template argument lists and a dependent call, with runtime
      selection and GCC/RCC Itanium symbol parity on i686/AMD64.
- [x] Handle an explicit argument list for a function template with two
      consecutive type packs according to GCC's partition (all explicitly
      supplied arguments bind to the first pack; the second remains empty).
      Verify the competing type-pack/non-type-pack overloads, runtime result,
      and GCC/RCC Itanium symbols on i686/AMD64.
- [x] Preserve a trailing defaulted non-type argument after an explicitly
      supplied type pack when selecting against a competing non-type-pack
      overload; verify defaulted and non-type-pack manglings and runtime
      selection against GCC C++20 on i686/AMD64.
- [ ] Audit fixed/defaulted parameters before packs and more complex explicit
      argument partitioning; preserve ambiguity and non-match behavior against
      GCC C++20.
  - [x] Verify candidate-specific explicit arguments with a fixed non-type
        parameter before a type pack, plus deduced/defaulted fixed type and
        non-type parameters before the pack. Check the GCC C++20 runtime oracle
        and RCC i686/AMD64 generation plus x64 execution through
        `test-cxx-function-template-overloads`; broader partitioning remains
        open.
  - [x] Evaluate `sizeof...(Pack)` in constexpr function-template calls using
        per-parameter pack counts, including empty/nonempty type and non-type
        packs, adjacent packs, and an overload with a trailing defaulted
        template parameter. Accept unnamed expanded function parameters while
        binding constexpr calls. GCC C++20 and the full
        `test-cxx-function-template-overloads` gate pass on i686/AMD64 with
        runtime execution.
  - [x] Clone and substitute function-template parameter default arguments
        with the active specialization's per-parameter pack counts, so
        `sizeof...(Pack)` is concrete before semantic analysis. Cover empty and
        nonempty type and non-type packs; strict GCC C++20 and the full
        `test-cxx-function-template-overloads` gate pass on both target widths
        with runtime execution.
- [x] Keep `test-cxx-function-template-explicit-specialization` in
      `CXX_REGRESSION_TARGETS`; verify it is reached by `test-cxx` and the
      Linux production `test-ci` workflow, and that native-Windows `test-cxx`
      includes the same gate.

## RCC continuation (2026-10-29, volatile i686 wide division DWARF)

- [x] Emit inline parameter values for signed and unsigned i686 64-bit variable
      division and remainder over volatile global and block-scope static
      operand pairs, including static TLS. Allow volatile reads only from
      address-backed storage. Atomic global/static sources and escaped
      automatic atomic pairs use their stable storage locations; volatile
      automatic operands remain unsupported. Verify scoped/global/TLS
      relocations and signed/unsigned boundary results in legacy and verified
      objects through `test-debug-info-inline-expression`.
- [x] Extend i686 inline division/remainder DWARF values to constant divisors
      over volatile global and TLS sources. Cover signed `-7` and unsigned `7`
      for both operations, verify relocation types/counts and full-width
      results in legacy and verified objects through
      `test-debug-info-inline-expression`.
- [ ] Extend computed division/remainder expressions to broader operand trees
      and atomic sources; expand optimized multi-piece location tracking.
  - [x] Extend i686 unsigned 64-bit constant-divisor division/remainder DWARF
        values to address-backed atomic global sources. Verify divisor 7,
        full-width values, and source relocations in legacy and verified
        objects through `test-debug-info-inline-expression`.
  - [x] Extend i686 unsigned 64-bit variable-divisor division/remainder DWARF
        values to atomic global dividend and divisor operands. Verify both
        operations, both relocations, and the full operand matrix in legacy and
        verified objects through `test-debug-info-inline-expression`.
  - [x] Extend i686 signed 64-bit variable-divisor division/remainder DWARF
        values to atomic global dividend and divisor operands. Verify signed
        results, both relocations, and the full operand matrix in legacy and
        verified objects through `test-debug-info-inline-expression`.
  - [x] Extend i686 signed 64-bit constant-divisor division/remainder DWARF
        values to address-backed atomic global sources. Verify divisor 7,
        full-width signed results, source relocations, and expression values
        for both direct and unary-negated atomic dividends, in normal and
        verified objects with the complete inline-expression verifier.
  - [x] Extend i686 signed 64-bit variable-divisor division/remainder DWARF
        values to escaped automatic atomic operand pairs. Read each operand
        from its frame location, emit no global relocations, and verify both
        operations across the signed operand matrix in normal and verified
        objects with the complete inline-expression verifier.

## RCC continuation (2026-10-29, non-type packs in class partial specializations)

- [x] Parse non-type pack expansions in class-template partial-specialization
  arguments, deduce each expanded value, and preserve the flattened primary
  template identity for empty, single-element, and multi-element tails.
  `test-cxx-class-template-specialization` passes GCC C++20, i686/AMD64 code
  generation, and AMD64 execution; the adjacent partial-order ambiguity,
  unequal-pack, and constraint-diagnostic gates also pass. The full `test-cxx`
  suite passes on this Windows host.

## RCC continuation (2026-10-29, member initializer scalar calls)

- [x] Defer direct member-constructor selection when a member initializer
      contains a bounded scalar function call whose declaration is not yet
      bound. Semantic analysis resolves the call and constructor, allowing
      potentially throwing calls to run while completed-base cleanup remains
      registered. `test-verified-cxx-throwing-temporary` passes with 58
      verified functions and zero fallback for i686/AMD64 at `-O0`/`-O2`;
      generated-object and legacy-host execution pass. The GCC C++20 oracle
      confirms ordinary and reference-bound initialization both unwind in
      order and return `6721` when the helper throws.

## RCC continuation (2026-10-29, nested member initializer calls)

- [x] Defer member-constructor selection for direct scalar calls with bounded
      argument expressions containing nested calls and no temporary cleanup.
      Keep nested argument calls subject to the typed lowerer's `noexcept`
      validation, while the outer call may throw and unwind completed bases.
      The focused gate passes with 59 verified functions and zero fallback
      for i686/AMD64 at `-O0`/`-O2`, including generated-object and legacy-host execution.
      GCC C++20 confirms ordinary and reference-bound construction both
      produce `6721` after the nested argument result is consumed.
- [x] Extend the bounded nested-call path to `noexcept` function-pointer
      callees and derive the call's exception property from its function type.
      Scope internal-linkage function relocations in verified global data so a
      static function pointer resolves to the emitted translation-unit-local
      definition. The focused gate passes with 61 verified functions and zero
      fallback for i686/AMD64 at `-O0`/`-O2`; verified-object execution checks
      both ordinary and reference-bound partial-construction unwind order
      (`7021`), and the legacy-host runtime passes.

## RCC continuation (2026-10-30, typed-SSA generic aggregate atomics)

- [x] Lower generic `__atomic_load`, `__atomic_store`, `__atomic_exchange`,
      and `__atomic_compare_exchange` for complete 1/2/4/8-byte struct/union
      objects, including packed under-aligned objects, using a same-width
      integer representation. Require trivial copyability for C++ classes and limit
      8-byte operations to AMD64; preserve scalar-only `_n` and legacy `__sync`
      forms. The implementation reads and writes the full object
      representation, including padding, and uses the existing observable
      atomic load, exchange, and compare-exchange IR operations. For packed
      aggregate loads, locked zero-to-zero compare-exchange returns the
      observed bytes atomically without changing the representation.
- [x] Add runtime coverage for successful and failed aggregate compare-exchange
      plus aggregate load/store/exchange. `test-verified-atomic-fetch-add`
      passes i686/AMD64 compilation at `-O0`/`-O2` with zero fallback, AMD64
      object execution, and GCC C17 oracles; scalar, pointer, and floating
      atomic regressions in the same target also pass. The host runner maps
      initialized `.data` and zero-filled `.bss` targets for local relocations
      while keeping generated text executable and writable data separate.
- [x] Lower automatic volatile C struct/union locals without an initializer
      when accesses are member-wise; this includes the generic aggregate
      atomic builtins. The aggregate fixture verifies both targets at `-O0`/
      `-O2` and executes AMD64 objects.
- [x] Lower initialized automatic volatile C struct/union locals by building
      the initializer in temporary storage and performing volatile byte
      stores into the declared object. The generic aggregate fixture checks
      runtime values for initialized struct and designated-union cases.
- [x] Copy whole automatic volatile C struct/union objects between volatile
      and ordinary locals using volatile object-representation accesses; the
      fixture checks both directions for direct and pointer-based struct and
      union copies, plus bit-field object representations.
- [x] Lower block-scope static volatile C struct/union objects with static
      `.data` and zero-initialized `.bss` storage, including whole-object
      assignments and member reads/writes. Also cover nested volatile member
      structs/unions in initialized `.data` and zero-initialized `.bss` locals.
      Runtime coverage confirms their initial and updated values.
- [x] Lower automatic C struct/union aggregates containing nested volatile
      members. Preserve volatile field accesses while copying ordinary fields,
      and cover initializers, direct member/subaggregate access, and
      pointer-based whole-object copies. The aggregate gate passes i686/AMD64
      at `-O0`/`-O2` with zero fallback, AMD64 object execution, and GCC C17
      comparison; `test-verified-volatile` and `test-ir` pass.
- [x] Update the LICM local-load regression for the valid case where a
      hoisted load is folded to its dominating stored value and the dead load
      instruction is removed. `test-ir` passes both x86 and x64 IR/MIR and
      native encoder execution checks.

## RCC continuation (2026-10-31, typed-SSA under-aligned aggregate atomics)

- [x] Lower generic aggregate load/store/exchange/compare-exchange for packed
      2/4-byte objects on i686 and AMD64, plus packed 8-byte objects on AMD64.
      Test objects are deliberately placed at offset one. The verified backend
      compiles with zero fallback at `-O0`/`-O2`, executes AMD64 outputs, and
      matches the GCC C17 fixture oracle via `test-verified-atomic-fetch-add`;
      `test-ir` also passes.
- [ ] Support aggregate widths outside 1/2/4/8 bytes and 8-byte objects on
      i686, non-trivial C++ objects, and the full `std::atomic<T>` API and
      concurrency model. Volatile aggregate TLS setup remains a separate
      typed-SSA gap.

## RCC continuation (2026-10-31, verified scalar virtual-base construction)

- [x] Lower a complete-object C1 constructor that initializes a directly
      virtual-inherited scalar-only leaf base from forwarded scalar
      parameters. Require empty constructor bodies and no virtual-base
      destructor/cleanup. Verify fallback-free i686/AMD64 O0/O2 object
      execution, legacy i686/AMD64 object generation, AMD64 legacy runtime,
      and strict GCC C++20 syntax. `test-verified-cxx-virtual-base-value`
      passes on the Windows host.
- [x] Cover multiple scalar arguments from a verified complete-object
      constructor into separate virtual-base and derived fields; verify the
      argument-to-field mapping with fallback-free i686/AMD64 O0/O2 execution.
      `test-verified-cxx-virtual-base-value` passes.

## RCC scalability follow-up (2026-10-31, direct-image relocation lookup)

- [x] Replace repeated linear Module symbol and relocation scans with
      open-addressed indexes and insertion-ordered same-offset relocation
      chains. Direct RIN/RLL/DRV image resolution and relative-patch symbol
      lookup use these indexes while preserving duplicate diagnostics and
      deterministic output. The indexed resolution paths and scale measurements
      below pass.
  - [x] Add an open-addressed Module symbol-name index and use it for symbol
        insertion/update, weak/source/size metadata, relocation symbol
        resolution, and `.ro` undefined-symbol reference marking. The index
        stores array positions, so symbol-array growth keeps entries valid.
        `test-format-validation`, `test-direct-relocation`,
        `test-global-initializers`, and `test-tls` pass.
  - [x] Index relocation chains by source section and offset. Keep same-offset
        candidates in insertion order so image and TLS resolvers preserve
        duplicate detection and relocation-kind filtering. The same direct
        image, global initializer, format, and TLS gates pass.
  - [x] Add test-only Module symbol/relocation lookup, rehash, resolver-candidate,
        and relative-patch counters. Measure five-run CPU medians and tracked
        Module-owned capacities with 32-symbol/64-relocation and
        4096-symbol/8192-relocation fixtures. On Windows, construction used
        104/34469 symbol lookup probes and 321/55377 relocation-index probes;
        tracked production Module bytes were 33552/1139184, excluding the
        72-byte test-only counter tail. Large-image emission performed
        4096 relative patches and 4096 resolver candidate visits per format;
        five-run median RIN/RLL/DRV times were 8/15/10 ms with 98672/98672/98768
        artifact bytes. Small-fixture medians were 1/3/1 ms with
        1136/1136/1232 artifact bytes. The six generated RIN/RLL/DRV images
        were accepted by rinvalidate. Tracked bytes include Module-owned array
        capacities, symbol and relocation names, and Reloc nodes; allocator
        metadata and temporary emitter output buffers are excluded.
        `make SHELL=cmd.exe test-format-validation` passes.

- [x] `.ro`の`objfile_add_symbol()`はsymbolを追加するたびにlinked list末尾まで歩き、
      `objfile_add_reloc()`もsection内relocation listの末尾を毎回探す。さらに
      `objfile_add_string()`はstring table先頭から全既存文字列を比較する。さらにwriterは
      各relocationで`objfile_find_symbol()`のlinear scanを呼ぶため、symbol数S・relocation数R・
      distinct string数Tが大きいobjectの生成／読み込みでO(S² + R² + T² + R×S)の走査と
      文字列比較が発生する。**受入:** ObjectFile／ObjSectionにappend tailを持たせ、section
      indexを直接解決する。symbol名とstringのlookupをhash indexで有界化し、first-match規則、
      symbol／relocation／string tableの決定的な順序と`.ro` byte列を維持する。多数symbol・
      relocation・重複stringのfixtureでappend traversal／probe数とobject生成・読み込み時間を
      比較し、二次増加を解消する。
  - [x] Add symbol and per-section relocation append tails, plus a section-index
        side array for direct relocation ownership lookup. Preserve list and
        serialized order; `test-format-validation` passes.
  - [x] Add an open-addressed string-offset index while preserving first-match
        offsets, insertion order, and serialized string-table bytes. Rebuild the
        index when reading `.ro`; the regression round-trips 4096 unique strings
        and 128 symbols/relocations through `test-format-validation`.
  - [x] Add an open-addressed symbol-name index pointing to stable `ObjSymbol`
        nodes. Preserve the first-match behavior when duplicate names occur and
        use the index for relocation symbol resolution during `.ro` writing.
  - [x] Add test-only probe/append counters and small/large round-trip
        measurements to `test-format-validation`: 64 strings/32 symbols versus
        4096 strings/4096 symbols. The large fixture records 108674 string-index
        probes, 30825 symbol-index probes versus 16781312 legacy linear symbol
        comparisons. Direct tail appends perform zero traversal hops; the old
        list walks would have taken 16773120 hops per append list.
        The final large fixture tracks 958890 ObjectFile heap bytes, excluding
        allocator metadata and test buffers. Final Windows runs measured
        63-76/4-6 ms for build+write/read. Probe counts and serialized order are
        asserted; timings are reported without flaky limits.
