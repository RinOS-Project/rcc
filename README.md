2026-10-08 follow-up: Function-local destructible static objects now use per-object guards, supported first-use dynamic initialization, exception guard-abort cleanup, and `__cxa_atexit` registration in both native backends. `build-rcc` and `build-rcxx` pass. Exception retry, reverse destruction order, and RinOS process-exit acceptance remain open ([TODO](TODO.md)); no tests were run.

2026-10-08 follow-up: Destructible TLS class objects now receive per-thread guards, supported dynamic initialization on first use, guard-abort cleanup, and `__cxa_thread_atexit` registration in both native backends. `build-rcc` and `build-rcxx` pass. Per-thread ordering, retry, and RinOS thread-exit acceptance remain open ([TODO](TODO.md)); no tests were run.

2026-10-08: Both native backends now lower static reference temporaries in
thread-local storage with per-thread lazy guards and `__cxa_thread_atexit`
cleanup registration. `build-rcc` and `build-rcxx` pass. Per-thread lifetime
regressions and RinOS thread-exit runtime integration remain open in [TODO](TODO.md).

# rcc / rcc++ / aqc

RinOS専用の、LLVM/Clangに依存しないコンパイラ・リンカ・アーカイバです。
このrepositoryはRIN v3 toolchainの実装途中です。現時点でC17またはC++20への
完全準拠、最適化pipeline、セルフホストを達成したとは扱いません。

## 現在のv3契約

- target triple: `i686-unknown-rinos` / `x86_64-unknown-rinos`
- object: 64-bit section/symbol/relocationを持つ`.ro v2`
- archive: `.ro v2` memberを収録する`.ra v2`
- image/library: 256-byte header、64-bit RVA、typed import/exportを持つRIN v3
- driver: 256-byte headerとresource/match metadataを持つNDRV v3
- final `.rin/.rll/.drv`: 別processの`rinsign`によるRDS1署名が必須
- build contract: `RIN-BUILD-MANIFEST 1`のtarget/artifact/entry/signingをCLIと照合

秘密鍵をcompilerや成果物へ埋め込む経路はありません。最終出力には
`--sign-profile debug|release`、`--rinsign`、`--sign-key`、`--public-key`を明示します。
debug鍵はRinOSのdebug build profileからpathとして渡し、release鍵はrelease buildごとに
必ず明示します。compiler側の既定鍵や鍵materialはありません。unsigned/signedの一時成果物は
出力先directoryに排他的に作成し、署名成功とRDS1構造確認後に最終名へ原子的に公開します。
`--emit-unsigned-v3`はlinker/validatorの試験専用です。

## 実装済みの基盤

- 独自lexer/parser/semaとi386/AMD64 code generator
- `-E`, `-S`, `-c`, `-shared`, `-driver`, `-MMD`, `-MF`
- `-I`, `-D`, `-U`, `-nostdinc`, `-ffreestanding`
- 両target tripleと矛盾する`-m32/-m64`指定の拒否
- nested include、function-like/variadic macro、`#`/`##` replacement-list
  operators、C++20 `__VA_OPT__`、条件付きpreprocess
- C17 `_Static_assert`の整数定数式評価と失敗diagnostic
- x86_64 SysVの整数引数、基本scalar/aggregate load-store、global data
- i686 verified SSAでの64-bit整数をlow/highの検証済みi32 pairとして保持する
  経路。算術、比較、shift、narrow integer／pointer cast、conditional phi、
  cdecl引数とEDX:EAX戻り値までpairを維持し、scalar i64の誤った32-bit
  MIR流入を拒否します（[実装状況](docs/implementation-status-i686-wide-scalar-ssa-v1.md)）。
- direct RIN/NDRVとobject linkでのDATA/CODE/BSS symbol relocation、関数ポインタ
- 文字列literalのread-only `.rodata`分離と独立RVA mapping
- `.ro/.ra v2` reader/writer、typed import、依存libraryを扱う`rld`
- external signerを安全な引数配列で起動する最終v3出力
- debug/release署名profile、衝突しないprivate staging、失敗時の既存成果物保持
- Aquamarine Shader Language `.aq`からRinShader `RSH1`へのbounded native compiler
- C++ local reference bindingは、public baseへの明示的な
  `static_cast<Base&&>`を通じてもcomplete derived temporaryとcleanupを保持し、
  `make test-cxx-function-template-references`で両target生成とx64実行を確認します。
- C++ global finalizerは、初期化済みまたはzero-initializedの大きな配列を
  reverse-order runtime loopで破棄し、`make test-global-finalizers`で4101要素の
  明示的な空initializer付き／initializerなし配列の要素順と宣言順を両targetと
  x64 hostで確認します。

`-O1`以上には整数constant folding、短絡式・定数分岐の除去、bounded inline等があり、
typed SSA/MIR、mem2reg、GVN/DCEを使うverified backendも`-fverified-backend`で選べます。
ただしverified backendは未対応ASTで既存backendへ戻る部分があり、通常compileの既定経路
でもありません。DWARFはline/info/frame、stack variable、基本・aggregate type等を出力しますが、
完全なvariable location list、inline attribution、任意prologueのCFIは未完成です。
C++ frontendはtemplates、exceptions、bounded RTTI/`dynamic_cast`、static initialization等を
実装していますが、C++20全体への準拠は未達で、modules/coroutinesやABI・templateの広い
corner caseが残っています。

## ビルド

POSIX環境では次でhost toolchainを作成します。

```sh
make -j
```

`test-atomic-builtins`は生成したi686 codeも直接実行するため、hostの32-bit libc開発環境
（Debian/Ubuntuでは`gcc-multilib`相当）を必要とします。

生成物は`rcc`、`rcc++`、`rld`、`rar`、`aqc`です。`aqc`は同じRinOS
checkoutの公開RSH1 ABI／validatorを使うため、別配置では`RINOS_ROOT`を指定します。
Windows用既存binaryを更新せずに
検証する場合は、出力directoryを分離できます。

```sh
mkdir -p ../build/rcc/obj ../build/rcc/bin
make -j OBJDIR="$PWD/../build/rcc/obj" BINDIR="$PWD/../build/rcc/bin"
```

## 使用例

```sh
rcc --target i686-unknown-rinos -c -MMD -MF app.d -o app.ro app.c
rcc --manifest app.rinbuild --emit-unsigned-v3 -o app.rin app.c
rcc --target x86_64-unknown-rinos -S -o app.s app.c
rar r libsample.ra sample.ro
rld --target x86_64-unknown-rinos --shared \
  --dep rinbase.rll --import rin_log_write=rinbase.rll@function \
  --sign-profile debug --rinsign ../rinsign/rinsign --sign-key debug.pem \
  --public-key debug-public.der -o sample.rll sample.ro
aqc --dump-ir -o sample.rsh sample.aq
```

## 回帰試験

```sh
make test-static-assert
make test-aqc
make test-cxx-cli
make test-cxx-function-template-references
make test-global-finalizers
make test-atomic-builtins
make test-x86-wide-scalar
make test-link
make test-archive
make test-archive-link
make test-manifest
make test-signing
make test-sanitize
make test-driver-policy
make test-weak-link
make test-comdat-link
make test-object-width
make test-special-sections
make test-direct-relocation
make test-optimize
make test-generic
make test-initializer-overrides
make test-alignof
```

`test-driver-policy`はNDRVをinteger-onlyに保ち、浮動小数点型と
FPU/SIMD inline asm stateをcodegen前に拒否することを確認します。
`test-signing`は`rcc/rcc++/rld`の最終出力でdebug/release profile、空白やshell
metacharacterを含むsigner/output path、署名失敗時の既存成果物保持、不正signer出力の拒否、
同一出力への並行実行、およびstaging fileの確実な後始末を確認します。
`test-sanitize`はASan/UBSanとLeakSanitizerを有効にした別buildで、両archの大きな
translation unit、C++ class、preprocessor、成功・診断・署名失敗経路を検査します。
`test-atomic-builtins`は8/16/32/64-bit整数のload/store/exchange/CAS、fetch add/sub、
fetch and/or/xor/nand、fenceと`<stdatomic.h>` APIを両archで生成・linkし、i686/AMD64の
両生成コードを直接実行します。i686の64-bit操作はbaseline CPU featureのCMPXCHG8B retry
loop、AMD64はnative 64-bit命令を使用します。符号拡張、
幅ごとのwrap、compare-exchange失敗時のexpected更新、複数threadでの16/32/64-bit算術
およびbitwise原子性を検査します。pointerのload/store/exchange/CASも両archで直接実行し、
pointerへのfetch算術・bitwiseはSemaで拒否します。wide/pointer-sized型を含むC17 atomic typedefは
両archで公開し、i686の`long`は既存32-bit lock-free pathを使用します。定数memory orderの範囲、load/store制約、
compare-exchangeのfailure/weak制約もSemaで拒否します。
`test-x86-wide-scalar`はi686 SysVの64-bit整数について、EDX:EAX戻り値、8-byte
cdecl引数、literal、符号/ゼロ拡張、local/global load/store、加減算、bitwise演算、
signed/unsigned比較、SHLD/SHRD shift、multiply、software divide/modulo、
前置/後置increment/decrement、全integer compound assignment（左辺の一回評価を含む）、
内部関数callを32-bit host processで
直接実行します。未実装のwide演算は下位32-bitへ
暗黙切り詰めせずdiagnosticにします。
`test-integer-literals`はC17のdecimal/octal/hex候補型、`U/L/LL` suffix、ILP32/LP64
の型差、unsigned定数式の比較・wrap、64-bit即値codegenを両archで直接実行し、
候補型なし、64-bit overflow、不正suffixを各段階で拒否します。
`test-integer-promotions`はshift operandの独立promotion、左辺に基づく結果幅・signedness、
char/shortの単項promotionを両archで直接実行し、`%`、`~`、shift、`!`の不正operandを
意味解析で拒否します。
`test-integer-conversions`はcast、代入式、local初期化、return、固定引数callでの
8/16/32/64-bit整数変換と`_Bool`正規化を両archで直接実行します。AMD64の32-bit
算術wrapと、i686の高位wordだけが非zeroの64-bit値から`_Bool`への変換も検証します。
`test-function-calls`はprototype有無の区別、固定引数の個数・型検査、variadicと
prototypeなしcallのdefault integer promotion、array/function parameter調整を検証し、
固定・可変・間接callを両archで直接実行します。
`test-varargs`はi686のstack cursorとAMD64 SysVのGP register-save/overflow領域を使う
`va_start`/`va_arg`/`va_copy`/`va_end`を検証し、integer、pointer、64-bit scalar、
register枯渇後のstack引数を両archで直接実行します。浮動小数・aggregateの`va_arg`は
未対応のまま暗黙に誤生成せず、意味解析で拒否します。
`test-scalar-comparisons`は通常算術変換後のsigned/unsigned relational比較と、
高位wordだけが非zeroの64-bit整数を使う`!`、`&&`、`||`、条件演算子、if/loopの
truth判定を両archで直接実行します。
`test-aggregate-copy`はcompatible struct/unionのlocal copy初期化、通常・連鎖代入、
端数byte copyとC11/C17の匿名struct/union member layoutを両archで直接実行します。
`test-aggregate-returns`は
i686 hidden sretとAMD64のregister/sret aggregate return、戻り値のmember access、
aggregate引数への連鎖を両archで直接実行します。`test-bootstrap-core`は専用の
freestanding宣言sysrootを使い、stage0 rccでlexerを含むfrontend/sema/optimizer/backend/
preprocessor、object/assembly emitter、archive/linker、RIN/RLL/NDRV v3 packager、
C++ parser subset、build manifest、host process shim、4 CLI entry pointの25 translation unitを
両arch各2回compileして`.ro v2`のbyte一致を要求します。
`test-bootstrap-link`はこのうちrccのobject closureを`rincrt.rll`へのtyped import付き
RIN v3 executableへ両arch各2回linkし、imageのbyte一致を要求します。
`test-bootstrap-execute`はhost側の最小RIN v3 runnerでrelocation、typed import binding、
W^Xを適用して両archのstage1を実行し、probe sourceの`.ro v2`がstage0出力とbyte一致
することを要求します。別translation unitへの直接callは`REL32`、動的function importは
`rld`生成のcode thunkとloader書込みslotへ分離されています。
`test-bootstrap-stage2`は実行中のstage1で25 translation unitを再生成して全objectを
stage0出力と比較し、再linkしたstage2 RIN v3 imageにもbyte一致を要求します。これは
host bootstrapのgateであり、署名済みstage1のRinOSネイティブ実行完了宣言ではありません。
`test-compound-literals`はautomatic compound literalのscalar/array/aggregate storage、
postfix member/index、aggregate引数、initializerの一回評価を両archで直接実行します。
`test-compound-assignment`は全integer compound operator、通常のsigned/unsigned
divide/modulo、8/16-bit格納変換、左辺一回評価をi686/AMD64で直接実行します。
`test-switch-statement`は裸の`signed`/`unsigned`型指定、case/default dispatch、
fallthrough、nested switch、loop内のbreak/continue、制御式の一回評価、32/64-bit
case値を両archで直接実行し、switch外label、重複・非定数case、複数default、
非整数制御式を意味解析で拒否します。
`test-control-flow`はforward/backward `goto`、function-local label namespace、switchへの
直接遷移を両archの`-O1`生成コードで実行します。goto先を含む定数falseのif/whileを
optimizerが除去しないこと、および不正なbreak/continue、未定義・重複labelの診断も確認します。
`test-parser-recovery`は未知のparameter/field型とblock内の非消費tokenを一つのtranslation
unitで診断し、後続宣言まで有限時間で回復することを確認します。timeoutまたはsegmentation
faultは明示的に失敗とし、同じfixtureをASan/UBSan compilerにも通します。
`test-weak-link`は後続strong定義が先行weak定義のsection、binding、size、
最終RVAを完全に置換することを確認します。
`test-comdat-link`は`.ro v2`のCOMDAT ANY groupを入力順どおり一つだけ選択し、
group内section・symbol・relocationを一体で保持します。非COMDATのstrong重複と、
未対応selection、範囲外key、flagなしreserved metadataはfail closedにします。
`test-object-width`は4 GiB超の`.ro v2` symbol/addendとAMD64配置を保持し、
ABS32U/ABS32S overflow、legacy ABS32の新規出力、x86の3 GiB境界を拒否します。
`test-archive-link`は両archで未解決symbol駆動のmember選択と推移抽出を行い、
未使用member、入力順に対する過去archiveの再走査、異種arch member、および
symbol tableとmember実体が矛盾する改変archiveを拒否します。
`test-special-sections`はTLS、unwind、init/fini arrayを`.ro v2`からRIN v3へ
両archで保持し、TLS zero-fillとBSSのfile/memory sizeを分離します。またW^X、
array幅、同名section metadataの矛盾を拒否します。
`test-direct-relocation`は複数global、read-only RODATA、zero-file-size BSS、
関数ポインタが実symbol RVAへ解決されることをRIN/NDRVの両archで検査します。
extern、tentative
definition、重複・型衝突も検査し、未解決direct imageを拒否する一方、同じ参照を
`.ro v2`経由のmulti-object linkでは解決できることを確認します。
`test-verified-backend`は`-fverified-backend -c`で、対応済みscalar C/C++
translation unitをtyped SSA、MIR、SysV legalization、native encoderから`.ro v2`へ
直接出力します。static内部callのscoped symbolと`REL32`を保持し、pointer添字・加減算を
符号を保つscaled GEPへ、条件演算子と`&&`/`||`を短絡評価するCFG/phiへloweringします。
pointerの前後incrementと`+=`/`-=`も同じGEP契約を使い、副作用を持つlvalue addressは
一度だけ評価します。pointer差分はbyte差を要素サイズでsigned除算して`ptrdiff`要素数に
正規化します。`switch`はinteger promotion後の比較chain、case/default block、break、
fallthrough、nested switchをCFGへloweringし、case以前の文を実行しません。生成した
両archの`.text`はW^X mappingで直接実行して検証します。
global dataまたは未対応構文を
含む場合はtranslation unit全体を既存backendへ戻します。この切替は段階移行用の
明示optionであり、通常compileの既定出力はまだ変更しません。
`test-optimize`は`-O0`と`-O1`の両arch objectを比較し、整数constant folding、
短絡式、定数`if`、ゼロ回`while`のコード縮小と副作用除去、およびx86_64生成コードの
実行結果を確認します。
`test-generic`はC17 `_Generic`のsigned/unsigned、typedef、pointer、default、
array/function conversion、非評価controlを両archで検査し、重複association、
不完全type、matchなしを拒否します。
`test-initializer-overrides`はarray/struct/unionおよびネストdesignatorで、後続の
initializerが同じsubobjectを置換するC17規則をglobal/local・両archで検査します。
`test-alignof`はC17 `_Alignof(type-name)`をinteger constant expression、static
initializer、通常式として両archで検査し、不完全・function typeを拒否します。

RinOS親repositoryには、preprocessor、x86_64実行ABI、SDK v1 packaging、
production RIN v3 validatorを組み合わせた統合試験もあります。

## 完成条件

2026-10-08 follow-up: reference initialization now considers public implicit
conversion functions returning class references or class prvalues, preserves
the returned value category, and applies derived-to-base reference adjustment.
Generated implicit-object addresses now permit a class-prvalue receiver to be
materialized for its conversion call. Receiver cleanup plans stay active
through the containing full-expression, and class-prvalue reference arguments
use caller-frame temporary storage and cleanup on both target backends.
`test-cxx-function-template-references` covers lvalue-reference,
rvalue-reference, and class-prvalue conversion results, reference arguments,
derived-to-base adjustment, cleanup order, and prvalue receiver materialization.
`test-cxx-static-reference-temporaries` covers namespace and block-static
reference-bound scalar and class temporaries, guarded initialization, and
destructor registration on both target backends. Its i686 PE object-generation
and x64 host execution passed with one-time initialization and reverse-order,
exactly-once class destruction checks. The implementation also emits guard-
abort cleanup for exceptions during block-static initialization. The
`test-cxx-static-reference-retry` regression now checks generated cleanup
registration on both targets; this Windows host has only compiled the objects,
so retry execution and RinOS runtime integration remain unverified.
`test-cxx-static-reference-subobjects` passes i686/AMD64 object generation and
x64 host execution for direct member subobjects, conditional/comma class
sources, and explicit non-virtual/virtual base xvalue bindings, including
exactly-once destruction order. `test-cxx-member-pointer-data` now parses and
executes direct data-member pointers, lvalue/xvalue selection, assignment,
null values, implicit/explicit owner conversion, non-virtual multiple
inheritance, virtual-base application, and friend-authorized private-member
formation. It generates i686 and AMD64 objects, executes AMD64 output on the
host, and verifies all seven typed-IR functions on both targets. The same test
checks global and block-static reference lifetime extension through a
pointer-to-member-selected subobject and final destruction. Remaining work
includes member-function pointers, broader inherited/access-authorized forms,
virtual-base owner conversions, and further ABI/context coverage; see
[`TODO.md`](TODO.md).

The same regression now checks `&Derived::member` for a unique public member
inherited through both non-virtual and virtual bases, and verifies that the
result retains the declaring class as its pointer-to-member owner. Its negative
companion checks inaccessible member formation, ambiguous object application,
and ambiguous owner conversion on both targets. Member-function pointers,
hidden/non-public inherited lookup, and the broader access-context matrix still
need coverage. Owner conversions across virtual bases are ill-formed in C++.

The regression also checks member-pointee `const` addition (including a
derived-owner conversion), rejects const removal, requires an lvalue for
built-in scalar assignment through a selected xvalue, and keeps valid class
xvalue copy assignment.

公開toolchainとしての完成条件は、C17、主要C++20、typed SSA/MIRと最適化、
i386/AMD64 ABI、DWARF unwind、PIC/PIE、TLS/exception/RTTI、stage2再現build、
RinOS上の32/64-bitセルフホストです。進捗は[`TODO.md`](TODO.md)を参照してください。

2026-10-09: Function-template local classes now retain dependent base type
patterns until specialization, expand a type pack into concrete bases before
layout and virtual checks, and substitute packed member parameters and
pack-expanded base constructor initializers. Templated base initializer names
retain their type pattern so same-named base templates bind to the matching
specialization. `make build-rcxx` passed. The focused fixture and target objects
were not run. Specialized member declarations retain their source attributes
and function-type metadata, and resolve their owning class type before
publication. The broader local-class constructor/member ABI item remains open
in [`TODO.md`](TODO.md).

2026-10-09 follow-up: dependent local-class using-declarations now retain
`Base<T>` through parsing and resolve it against the selected base specialization.
This connects public direct `using Base<T>::Base` declarations to the existing
bounded inherited-constructor lowering. `make SHELL=cmd.exe -B build-rcxx`
passed; no fixture or target objects were run, so the parent ABI TODO remains
open.

2026-10-09 follow-up: inherited constructors for local class-template
specializations now retain supported constant scalar default member
initializers in addition to forwarding constructor arguments to a public direct
base. Class/array members and non-constant or out-of-range initializers remain
outside this bounded profile and are diagnosed. `make SHELL=cmd.exe
build-rcxx` passed; no tests were run for this change.

2026-10-09 follow-up: synthesized inherited constructors now retain constexpr,
consteval, nodiscard/deprecation, noreturn/inline, and function-type prototype
metadata from their base declaration. Variadic and prototype-less source
constructors are rejected instead of being emitted with a narrowed signature.
`gcc -Wall -Wextra -std=c11 -fsyntax-only` passed for `src/parser_cxx.c`; no
tests or full relink were run for this metadata correction.

2026-10-09 follow-up: inherited constructors now default-construct a derived
class-type field when its zero-argument constructor is lowerable and the field
has no destructor/cleanup obligation. Both target backends accept and emit this
empty-argument member-constructor call. Empty-brace and scalar-constant
direct-list default member initializers use the same path. Arrays, non-constant
or non-scalar class-field initializers, and destructor-bearing fields remain
outside the bounded path.
The inherited-constructor and local-class targets and full native-Windows
`test-cxx` aggregate pass, covering i686/x86_64 object generation and x64 host
execution for class-member defaults and dependent-base lookup.

2026-10-09 follow-up: fixed-size arrays of class members with lowerable
zero-argument element constructors now default-construct each element in order
through both x86 backends. The same loop is used from inherited-constructor
lowering and the ordinary constructor member-initializer prologue. Inherited
constructors still reject array DMIs, nested or incomplete arrays, and elements
that require cleanup. The forced compiler build passed; no runtime tests or
target objects were run, so the local-class constructor/member ABI TODO remains
open pending per-element runtime coverage.

2026-10-09 follow-up: ordinary class-template specializations now retain a
dependent direct base until specialization, resolve dependent `using` members
and inherited constructors, and lower `this->` field/method lookup. The new
`test-cxx-class-template-dependent-base` target covers `int` and `long long`;
the nested TODO records its x64 run and both target-width emissions. The full
compiler rebuild passed in this integration, but the target was not rerun.
Dependent base packs, virtual-base/override rules, access and ambiguity, and
remaining dependent member initializer forms stay unchecked in the parent.

2026-10-09 verification follow-up: the dependent-base fixture now checks
`T bias = -1`, an unsigned 32-to-64-bit initializer, and the full-width value
`0x100000001ULL`. `make SHELL=cmd.exe test-cxx-class-template-dependent-base`
passes freestanding i686 execution, x64 host execution, and both target object
generations. This verifies i686 sign/zero extension and high-word storage for
these forms; dependent base packs, virtual/access/ambiguity rules, and other
initializer forms remain unchecked.

2026-10-09 array-member verification: the inherited-constructor regression now
executes fixed-size class-array initialization through both inherited
constructors and ordinary constructor prologues. Side-effecting element
constructors verify declaration order, including empty-brace array default
member initializers. The fixtures pass freestanding i686 and AMD64 host
execution and emit both target objects. Negative cases continue to reject
nested arrays and inherited arrays whose elements require cleanup. The parser
and constructor lowerability checks admit only complete, one-dimensional arrays
with lowerable zero-argument constructors and no cleanup obligation. The broad
constructor/member ABI TODO remains open.
