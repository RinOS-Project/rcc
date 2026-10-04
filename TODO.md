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
- [x] `.ro v2`契約内のCOMDAT、weak symbol、archive選択規則
    - [x] `.ro v2` COMDAT ANY groupの決定的選択と破損metadata拒否
    - [x] weak→strong置換時のsection/binding/size/RVA更新
    - [x] 入力順を保つ未解決symbol駆動の`.ra v2` member推移選択
    - [ ] 他形式・全ABI edge caseを含む完全なCOMDAT/weak/archive互換性
- [x] `.ro v2`からRIN v3へ伝播するTLS、INIT/FINI、UNWIND section metadata
    - [x] `.ro v2` typed sectionのRIN v3伝播、W^X/幅/metadata検証
    - [x] BSSとTLS zero-fill tailのfile size / memory size分離
    - [ ] 実行時TLS destructorと完全なUNWIND personality/runtime連携

## 2. C17 frontend

- [x] 基本declaration、function、struct/union/enum/typedef
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
- [x] `f` suffix付き浮動小数点リテラルの型保持と、定数式による
      float/doubleのstatic/TLS IEEE scalar初期化
- [x] x86-64 runtimeのfloat/double算術・比較・cast・代入と、
      register-only SysV XMM scalar引数/戻り値lowering
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
  - [x] `restrict`をpointer自体へ保持し、object/incomplete typeを指す制約、
        function pointer・非pointer適用の明示diagnosticを型名・宣言・member・
        `sizeof`/cast経路と両arch回帰で検証
  - [x] nested pointer cv qualification conversionで、直接pointeeの修飾追加を
        維持しつつ、保護されていない内側levelの危険な修飾追加・破棄を拒否。
        const-protected intermediate pointerとi686/AMD64のdiagnosticを回帰検証
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
- [x] bounded `_Generic`、atomics、thread-local storage
  - [x] Lower the commonly used GCC compatibility builtins
          `__builtin_expect` and `__builtin_unreachable` as validated intrinsics
          on i686/AMD64, including real undefined-path trapping and C/C++
          compile-and-run coverage.
  - [x] Lower `__builtin_trap` as a validated no-argument terminating
        intrinsic with real i686/AMD64 trap instructions and diagnostic
        coverage.
  - [x] Lower common scalar builtins `__builtin_bswap{16,32,64}`,
        `__builtin_{clz,ctz,popcount}{,ll}`, and `__builtin_prefetch` with
        target instructions, constant-argument validation, and dual-arch C/C++
        regression coverage.
  - [x] `_Generic`のcompatible type選択、default、非評価control
  - [x] 8/16/32-bit整数atomic load/store/exchange/CAS/fetch add/sub/bitwiseとfull fenceの両arch codegen
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
- [x] C17 conformance compile-and-run suite

## 3. C++20 frontend / ABI

- [x] `rcc++` entrypointとC++20既定mode
- [x] C frontendと共通のtarget/preprocessor CLI
- [x] bounded class、継承、virtual dispatch実装
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
  - [ ] 標準C++の全class layout、特殊メンバー、virtual ABI互換性
- [x] bounded overload resolution、namespace、ADL、two-phase lookup
  - [x] target幅`nullptr_t`、`auto`保持、null-pointer conversion、条件式・overload
  - [x] 宣言側default argument、再宣言累積、overload viability、call-site補完
  - [x] parser-knownなnamespace所属class型の引数からqualified symbolをADLで
        解決し、free function callをi686/AMD64で実行検証
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
  - [ ] 標準C++の全conversion rank、ADL、two-phase lookup互換性
- [x] bounded templates、concepts、constexpr/consteval、lambda
  - [x] bounded type/non-type parameter packs、pack expansion、fold expression、
        empty-pack identity、pack-based static membersの両arch回帰
  - [x] C++20 named conceptを`template<Concept T>`制約付きtype parameterへ
        適用し、instantiation時のconcept評価とC++17以前のstandard gateを検証
  - [x] parser-known型によるdirect/pointer function-template deductionと
        trailing type defaultの実体化
  - [x] 先行非型引数を参照する整数constant-expression defaultの評価
  - [x] dependent aggregate templateの未対応partial loweringを診断
  - [x] C++20 aggregate parenthesized initializationを完全なpublic struct
        aggregateと固定長配列へloweringし、static/automatic storageの両arch
        実行、過剰initializer診断、C++17以前の明示的standard-gateを検証
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
  - [ ] full partial ordering, all parameter-pack deduction rules, and
        unsupported constexpr evaluation required for complete standard
        conformance
- [ ] C++20 modules、coroutines
- [x] bounded Itanium ABI mangling、exceptions、RTTI、static initialization
- [x] cross-library exception transport and cleanup across `.rll` boundaries
- [ ] remaining full Itanium ABI、RTTI/typeid、complete static/TLS destructor semantics
- [ ] thread-local destructor and exception cleanup interaction

## 4. IR / optimization

- [x] scalar typed SSA IRとCFGの検証済みsubset
  - [x] scalar/pointer SSA value、basic block、phi、terminator、dominance/use-def/type verifier
  - [x] scalar ASTのalloca/load/store、scaled pointer GEP、短絡条件・論理式SSA loweringとif/while/do/for/switch CFG verification
  - [x] non-escaping entry scalar allocaのdominance-frontier mem2regとphi挿入
- [x] 定数条件分岐のSSA branch化、到達不能blockと不要phi入力の除去
- [ ] aggregate/vector/exceptionを含む全frontendのtyped SSA lowering
  - [x] i686 cdeclの64-bit整数を、EDX:EAXのverified SSA return-pair、8-byte引数、
        local/global load、narrow cast、加減算のcarry/borrow、bitwise、
        signed/unsigned比較、0..63-bit shiftへ接続し、実行回帰で検証
  - [ ] i686 wide-scalarのcall、mul/div/mod、完全な
        first-class two-word SSA value model
- [x] target-independent MIR
  - [x] virtual register/block/phi/callを持つscalar MIRとIR→MIR shadow lowering/verifier
  - [x] critical-edge分類とcycle-safe parallel-copy schedulingによるphi edge lowering
- [x] bounded constant propagation / folding
  - [x] `-O1..3`での型範囲を守るAST整数constant foldingと短絡式除去
  - [x] 8/16/32/64-bit unsigned modulo演算・shift・比較・narrow cast folding
  - [x] alias/control-flow barrier付きblock-local整数constant propagation
  - [x] typed SSA整数演算・比較・castのconstant folding
  - [x] typed SSAの同一定数PHI／定数条件SELECT foldingとverifier/native回帰
  - [x] 変更・escapeのない局所整数に限定したwhile/do-while/forの
        loop-invariant constant propagationと両arch実行回帰
  - [x] side-effect-free integer algebraic identities (`+0`、`-0`、`*1`、`/1`、
        bitwise identity、zero folding)を型互換性と副作用保持付きで実装し、
        両archの最適化・実行回帰へ接続
  - [x] unsigned integerの`x * 2^k`／`2^k * x`、`x / 2^k`、`x % 2^k`を
        型付きshift/maskへstrength reductionし、i686/x86_64の即値shift生成と
        実行回帰を追加。signed/overflow-sensitive formは変更しない
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
  - [x] 副作用なし・引数なし・単一整数returnの直接呼出しをO1で限定inline
  - [x] 副作用なし・単一整数returnの純粋整数式を最大8個の引数へ展開し、
        各引数の評価を一回に限定したO1 inlineと両arch実行・call除去回帰
  - [x] 一意なpreheaderを持つ自然ループに対して、純粋typed-SSA命令の
        ループ不変性を支配関係とuse-defで検証してpreheaderへ移動する限定LICMを
        O2/O3へ接続し、移動後のverifierとIR回帰を追加
  - [x] 副作用のない単純整数識別子／リテラル引数が関数本体で複数回参照される
        場合も、複雑式のAST共有は行わず安全にO1 inlineし、両archでcall除去と
        実行結果を回帰検証
  - [x] 宣言順に依存しない最大8回の限定固定点で、純粋スカラーinline候補の
        前方呼出しチェーンを解決し、再帰・aggregate・exception callはこのpassの
        対象外として明示的に保持
  - [ ] 一般のloop transformation、recursive/cost-aware inline、aggregate/exception
        callのinline
- [x] `-O0..3`ごとのpass pipeline
  - [x] O0検証のみ、O1 mem2reg/fold/DCE、O2 GVN追加、O3固定点反復
  - [x] rcc/rcc++共通の厳密な`-O0..3` CLI検証と範囲外fail-closed
- [x] bounded linear-scan register allocation
  - [x] phi edge/call crossing対応MIR live intervalとpolicy駆動linear-scan/spill配置
  - [x] 非レイアウト順successor/back-edge対応CFG fixed-point liveness
  - [x] DIV/REMのAX:DXと可変shiftのCXを命令位置だけ予約するfixed-register制約
  - [ ] graph-coloring allocation and whole-program spill heuristics

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
- [x] bounded PIC/PIE、GOT/PLT、TLS relocation
  - [x] direct/internal、GOT/PLT、local-exec TLSの両arch relocation検証
  - [ ] 全visibility、interposition、TLS model、shared-library ABI互換性
- [ ] DWARF debug/unwind
  - [x] `-g`でrelocatable `.ro`へ関数開始・source file・lineを持つ最小DWARF
        `.debug_line`を出力し、両archのread/link回帰を追加
  - [x] 最小compile unit／subprogram DIE、`.debug_abbrev`／`.debug_str`、
        `low_pc` relocationを追加し、両archのobject/link回帰へ接続
  - [x] `DW_TAG_formal_parameter`／`DW_TAG_variable`へ実在するC/C++ stack
        declarationの名前とEBP/RBP相対`DW_OP_breg` locationを出力し、
        i686/AMD64 object・RLD link回帰で検証
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
  - [x] subprogram DIEへ実在する関数戻り型の`DW_AT_type`参照を追加し、
        AST型を持たない生成関数には型を捏造せず型無しabbrevを選択する
        両archのdebug-info回帰を追加
  - [ ] `.debug_info`の型／local variable／inline attributionと完全なCFI/unwind
- [ ] inline asm constraintの完全検証
  - [x] bounded i686/AMD64固定レジスタ制約、出力lvalue・scalar型、clobber、
        未実装placeholderをsemaで明示診断し、未知制約の黙殺を禁止
  - [x] 固定レジスタoperand同士、operandとclobber、重複clobberの衝突を
        backendへ渡す前に診断し、既存の`=a`出力と`a`入力のtie相当だけを
        維持する両arch回帰を追加
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
- [x] CI regression gateでC/C++ global initializer/finalizerのhost実行、
      `.init_array`/`.fini_array`伝播、RIN/RLL/DRV/RLD image validationを常時gate
- [ ] clang/gcc互換の全golden `.ro/.ra/.rin/.rll/.drv` corpusとfuzz corpus
  - [x] bounded C17/C++20 property corpusで両archの再生成一致と不正入力の
        明示`error:`診断をCI gateする
  - [x] RCC単体checkoutから実行できるbounded C17/C++20 golden manifestで、
        i686/AMD64の`.ro`、unsigned-v3`.rin`、`.rll`、`.drv`を2回再生成し、
        SHA-256をCIで固定検証する
  - [x] RCC単体checkoutから実行できる決定的parser/compiler fuzz gateで、
        C17/C++20の有効変異を両archで再現コンパイルし、無効変異のsignal／
        timeout／空診断／誤った成果物を拒否する
- [x] host stage0 -> rcc stage1 -> rcc stage2再現build
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
