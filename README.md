# rcc / rcc++

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
- nested include、function-like/variadic macro、条件付きpreprocess
- C17 `_Static_assert`の整数定数式評価と失敗diagnostic
- x86_64 SysVの整数引数、基本scalar/aggregate load-store、global data
- direct RIN/NDRVとobject linkでのDATA/CODE/BSS symbol relocation、関数ポインタ
- 文字列literalのread-only `.rodata`分離と独立RVA mapping
- `.ro/.ra v2` reader/writer、typed import、依存libraryを扱う`rld`
- external signerを安全な引数配列で起動する最終v3出力
- debug/release署名profile、衝突しないprivate staging、失敗時の既存成果物保持

`-O1`以上では安全な整数constant folding、短絡式・定数分岐の除去を行いますが、各levelの
SSA最適化pipelineと完全なDWARF生成は未完成です。C++ frontendも実験段階で、classの基本構文を
越えるtemplates、exceptions、RTTI、modules、coroutines等は完成していません。

## ビルド

POSIX環境では次でhost toolchainを作成します。

```sh
make -j
```

`test-atomic-builtins`は生成したi686 codeも直接実行するため、hostの32-bit libc開発環境
（Debian/Ubuntuでは`gcc-multilib`相当）を必要とします。

生成物は`rcc`、`rcc++`、`rld`、`rar`です。Windows用既存binaryを更新せずに
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
  --sign-profile debug --rinsign ../scripts/rinsign.py --sign-key debug.pem \
  --public-key debug-public.der -o sample.rll sample.ro
```

## 回帰試験

```sh
make test-static-assert
make test-cxx-cli
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
`test-scalar-comparisons`は通常算術変換後のsigned/unsigned relational比較と、
高位wordだけが非zeroの64-bit整数を使う`!`、`&&`、`||`、条件演算子、if/loopの
truth判定を両archで直接実行します。
`test-aggregate-copy`はcompatible struct/unionのlocal copy初期化とC11/C17の匿名
struct/union member layoutを両archで直接実行します。`test-bootstrap-core`は専用の
freestanding宣言sysrootを使い、stage0 rccで閉じたfrontend/sema/optimizer/backend/
preprocessor/C++ parser subsetを両arch各2回compileして`.ro v2`のbyte一致を要求します。
これはobject gateであり、linked stage1やstage2再現buildの完了宣言ではありません。
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

公開toolchainとしての完成条件は、C17、主要C++20、typed SSA/MIRと最適化、
i386/AMD64 ABI、DWARF unwind、PIC/PIE、TLS/exception/RTTI、stage2再現build、
RinOS上の32/64-bitセルフホストです。進捗は[`TODO.md`](TODO.md)を参照してください。
