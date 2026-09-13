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
- [x] `f` suffix付き浮動小数点リテラルの型保持と、定数式による
      float/doubleのstatic/TLS IEEE scalar初期化
- [x] x86-64 runtimeのfloat/double算術・比較・cast・代入と、
      register-only SysV XMM scalar引数/戻り値lowering
- [x] i686 runtimeのfloat/double x87算術・比較・cast・代入・前後置更新と、
      cdecl stack引数/スカラー戻り値lowering
- [x] nested include、macro、条件付きpreprocess、`-MMD/-MF`
- [x] `_Static_assert`整数定数式と失敗diagnostic
  - [x] `_Alignof(type-name)`の定数式・static/runtime codegen
- [ ] qualifierとeffective typeの完全なC17規則
  - [x] 前置・後置cv指定、pointer level cv、modifiable lvalue検査
  - [x] `volatile` object/pointerの未使用readに対するDCE抑止
- [ ] VLA、compound literal、designator列・brace省略・上書きを含む初期化子の完全実装
  - [x] brace省略列でbraced／文字列aggregate節を一つのsubobjectとして
       扱い、その後のscalar節を次のsubobjectへ進める。未指定長の多次元
       配列bound推論と、static／automaticの両arch実行を検証
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
    - [x] block-scope variably modified typedefをloweringし、linkageを持つ
          variably modified objectとstruct/union memberを両archで診断
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
- [ ] `_Generic`、atomics、thread-local storage
  - [x] `_Generic`のcompatible type選択、default、非評価control
  - [x] 8/16/32-bit整数atomic load/store/exchange/CAS/fetch add/sub/bitwiseとfull fenceの両arch codegen
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
- [ ] C17 conformance compile-and-run suite

## 3. C++20 frontend / ABI

- [x] `rcc++` entrypointとC++20既定mode
- [x] C frontendと共通のtarget/preprocessor CLI
- [ ] class、継承、virtual dispatchの完全実装
  - [x] 非static・非virtualメンバー関数の`this`引数、暗黙field参照、
        `obj.method`／`ptr->method`呼び出しと両arch実行
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
- [ ] overload resolution、namespace、ADL、two-phase lookup
  - [x] target幅`nullptr_t`、`auto`保持、null-pointer conversion、条件式・overload
  - [x] 宣言側default argument、再宣言累積、overload viability、call-site補完
  - [x] parser-knownなnamespace所属class型の引数からqualified symbolをADLで
        解決し、free function callをi686/AMD64で実行検証
- [ ] templates、concepts、constexpr/consteval、lambda
  - [x] parser-known型によるdirect/pointer function-template deductionと
        trailing type defaultの実体化
  - [x] 先行非型引数を参照する整数constant-expression defaultの評価
  - [x] dependent aggregate templateの未対応partial loweringを診断
- [ ] modules、coroutines、atomics、TLS
- [ ] Itanium ABI mangling、exceptions、RTTI、static initialization
- [ ] cross-library exceptionとthread-local destructor

## 4. IR / optimization

- [ ] typed SSA IRとCFG
  - [x] scalar/pointer SSA value、basic block、phi、terminator、dominance/use-def/type verifier
  - [x] scalar ASTのalloca/load/store、scaled pointer GEP、短絡条件・論理式SSA loweringとif/while/do/for/switch CFG verification
  - [x] non-escaping entry scalar allocaのdominance-frontier mem2regとphi挿入
- [x] target-independent MIR
  - [x] virtual register/block/phi/callを持つscalar MIRとIR→MIR shadow lowering/verifier
  - [x] critical-edge分類とcycle-safe parallel-copy schedulingによるphi edge lowering
- [ ] constant propagation / folding
  - [x] `-O1..3`での型範囲を守るAST整数constant foldingと短絡式除去
  - [x] 8/16/32/64-bit unsigned modulo演算・shift・比較・narrow cast folding
  - [x] alias/control-flow barrier付きblock-local整数constant propagation
  - [x] typed SSA整数演算・比較・castのconstant folding
- [x] mem2reg、DCE、CSE/GVN
  - [x] 定数`if`分岐選択とゼロ回`while/for`のAST dead-code除去
    （`goto`および`case/default`からのentryを保持）
  - [x] block内control transfer後の直列DCEとlabel/case entry保持
  - [x] 未使用の副作用なし式文DCEとcall/volatile/C++ cleanup保持
  - [x] escape/control-flow barrier付きblock-local整数dead-store除去
  - [x] side effectのない未使用SSA定義の再帰的除去
  - [x] alias-free整数・cast・GEP・select・symbol addressのbasic-block内CSE
  - [x] dominator scopeと兄弟分岐隔離を持つSSA global value numbering
- [ ] loop optimization、inlining
- [x] `-O0..3`ごとのpass pipeline
  - [x] O0検証のみ、O1 mem2reg/fold/DCE、O2 GVN追加、O3固定点反復
  - [x] rcc/rcc++共通の厳密な`-O0..3` CLI検証と範囲外fail-closed
- [ ] linear-scan / graph-coloring register allocation
  - [x] phi edge/call crossing対応MIR live intervalとpolicy駆動linear-scan/spill配置
  - [x] 非レイアウト順successor/back-edge対応CFG fixed-point liveness
  - [x] DIV/REMのAX:DXと可変shiftのCXを命令位置だけ予約するfixed-register制約

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
- [x] SysV aggregate分類、floating-point/aggregate variadic ABI
- [x] i686 cdeclとAMD64 SysVのfloating scalar variadic引数について、既定昇格、
      XMM register-save領域、`va_arg`のregister/overflow経路を実装
- [ ] PIC/PIE、GOT/PLT、TLS relocation
- [ ] DWARF debug/unwind
- [ ] inline asm constraintの完全検証
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
- [ ] golden `.ro/.ra/.rin/.rll/.drv`とfuzz corpus
- [x] host stage0 -> rcc stage1 -> rcc stage2再現build
  - [x] stage0による閉じたfrontend/sema/backend subset `.ro`の両arch再現生成
  - [x] build manifest、host process shim、rcc/rcc++/rld/rar entry pointまでの再現object生成
  - [x] `rincrt.rll` typed import付きrcc stage1 RIN v3 imageの両arch再現link
  - [x] host RIN v3 runnerで両arch stage1を実行し、probe `.ro v2`のstage0一致を検証
  - [x] stage1によるcompiler全translation unitのstage2再生成とimage一致
- [ ] RinOS i686セルフホスト
- [ ] RinOS x86_64セルフホスト
