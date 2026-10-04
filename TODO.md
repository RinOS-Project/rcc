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
    - [x] C17の旧式identifier-list function declaration/definitionを型付き
          parameter declarationへ接続し、既定int・array/function parameter
          adjustment・未知parameter診断と両arch compile/run回帰を追加
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
        `__builtin_{clz,ctz,popcount}{,l,ll}` into verified typed-SSA
        operations with deterministic zero handling, target-width validation,
        dual-arch emission, and x86_64 execution coverage. `__builtin_prefetch`
        is also lowered to validated x86 read/write prefetch hints with
        optional-argument defaults and dual-arch object/runtime coverage.
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
  - [x] `()`, `[]`, `{}`の深さを追跡してネスト内の誤った同期点を避け、
        C17/C++の宣言開始点と後続宣言の回復を両archでタイムアウト・
        クラッシュなしに検証
  - [x] missing terminatorでcursorがtypedef名、C++ `bool`/`char8_t`、
        `requires`等の後続宣言開始点に残った場合、そのtokenを先に消費せず
        次のparse iterationへ渡し、連続無進捗時だけ確実に同期消費する回復を
        C17/C++20の診断fixtureで検証
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
  - [x] `typeid(T)`と非多相式の静的typeinfo identityをi686/AMD64で生成し、
        同一型のidentity共有・異なる型の分離を実行回帰。
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
- [x] cross-library exception transport and cleanup across `.rll` boundaries
- [ ] remaining full Itanium ABI、`type_info` API、complete static/TLS
      destructor semantics
- [ ] thread-local destructor and exception cleanup interaction

## 4. IR / optimization

- [x] scalar typed SSA IRとCFGの検証済みsubset
  - [x] scalar/pointer SSA value、basic block、phi、terminator、dominance/use-def/type verifier
  - [x] scalar ASTのalloca/load/store、scaled pointer GEP、短絡条件・論理式SSA loweringとif/while/do/for/switch CFG verification
  - [x] non-escaping entry scalar allocaのdominance-frontier mem2regとphi挿入
- [x] 定数条件分岐のSSA branch化、到達不能blockと不要phi入力の除去
- [x] cleanup/VLA跨ぎを伴わないC goto/labelを事前収集したSSA CFG blockへ
      lowerし、i686/AMD64 verified backend emit回帰を追加。cleanup/VLA跨ぎは
      引き続き明示的にverified subset外として扱う。
- [ ] aggregate/vector/exceptionを含む全frontendのtyped SSA lowering
  - [x] i686 cdeclの64-bit整数を、EDX:EAXのverified SSA return-pair、8-byte引数、
        local/global load、narrow cast、加減算のcarry/borrow、bitwise、
        signed/unsigned比較、0..63-bit shift、direct call、div/modへ接続し、
        実行回帰で検証
  - [ ] i686 wide-scalarの完全な
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
  - [x] 副作用のない整数引数がinline本体で複数回参照される場合も、対応する
        式木を複製し、引数ごとの展開コストを16ノード以内に制限してO1 inlineへ
        接続。i686/x86_64のcall除去と実行結果を回帰検証
  - [x] 副作用のない整数return式のcast／条件演算子を式木複製の対象へ拡張し、
        型情報を保持したO1 inlineとi686/x86_64のcall除去・実行結果を回帰検証
  - [x] 純粋整数inlineのreturn式全体を64ノード以内に制限し、複数の引数置換で
        上限を迂回しない展開コスト計算と、上限超過時のcall保持を回帰検証
  - [x] compile-timeに1回だけ実行される副作用なし`for`を、`break`／`continue`／
        `goto`／labelを含まないことを確認してblockへbounded unrollし、両archの
        code-sizeと実行結果を回帰検証
  - [x] 定数初期値・定数境界で必ず0回になる副作用なし`for`を、符号付き／符号なし
        整数の比較規則を保持してinitializerだけのblockへ縮約し、両archの
        code-sizeと実行結果を回帰検証
  - [x] 定数`while (0)`の`do`本体を、loop-transferを含まない場合だけ一回実行の
        bodyへ縮約し、`continue`を含む本体は保持したまま両archの実行を回帰検証
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
  - [x] `__builtin_expect`を副作用順序付き値伝播へ、`__builtin_unreachable`／
        `__builtin_trap`をtyped-SSA終端と実UD2へlowerし、i686/AMD64の
        verified backend fallbackなし回帰を追加
  - [x] `__builtin_bswap16/32/64`をtyped-SSAのmask／shift／論理演算へlowerし、
        x64の16/32/64-bit実行値、i686の16/32/64-bit fallbackなしを
        回帰検証
  - [x] i686 wide-scalarの代入、複合代入、pre/post incrementをpair
        load/storeとcarry/borrow付き演算へlowerし、両archのobject・x64
        実行回帰で検証
  - [x] i686 wide-scalarのtruth、論理否定、short-circuit AND/ORを既存の
        two-word truth reductionとCFGへ接続し、両archのobject・x64実行で検証
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
  - [x] 現行のframe-pointer prologue／epilogueに対応するDWARF32
        `.debug_frame`のCIE/FDEをrelocation付きで出力し、CFA・saved FP・
        return address規則とloadable imageからの除外を両archで検証。非標準
        prologueの完全なCFIは引き続き未実装
  - [x] CIE/FDEのPC進行を実際の`push fp; mov fp,sp`完了位置へ合わせ、
        epilogueのCFA復帰とsaved FP復元を両archのdebug-info回帰で検証
  - [x] 非再帰のstruct／union／固定長array／vector型について、実フィールド・
        要素型・byte size・member locationを持つDWARF type DIEを出力し、
        i686/AMD64のobject・link回帰で検証。bit-fieldにはbit size／offsetも
        出力し、aggregate先頭からのdata bit offsetを記録する。
  - [x] 再帰struct／unionのmember type参照をDIE生成後のforward-reference
        patchで解決し、自己参照pointerが実aggregate DIEを指すことを
        i686/AMD64のobject debug-info回帰で検証
  - [x] enum型と列挙子へ`DW_TAG_enumeration_type`／`DW_TAG_enumerator`と
        signed constant valueを出力し、i686/AMD64のobject・link回帰で検証
  - [x] 非再帰function typeへ`DW_TAG_subroutine_type`と戻り型・parameter
        type DIEを出力し、再帰function typeのforward referenceもpatchで
        解決してi686/AMD64のdebug-info回帰で検証
  - [x] const／volatile／restrict／atomic修飾型を対応するDWARF qualifier
        DIEと実在する基底型参照へlowerし、i686/AMD64のglobal variableと
        linked-image debug-info回帰で検証
  - [ ] `.debug_info`の型／local variable／inline attributionと完全なCFI/unwind
- [ ] inline asm constraintの完全検証
  - [x] bounded i686/AMD64固定レジスタ制約、出力lvalue・scalar型、clobber、
        numeric `%N` placeholderの固定レジスタ展開、`%%` escape、および
        `mov source,destination`の実バイト生成を実装。名前付き／modifier／
        unsupported operandは明示診断し、未知制約の黙殺を禁止
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
- [x] Make C++ language-linkage symbol checks use the shared host text helper,
      and run namespace parser-recovery diagnostics through a bounded Windows
      PowerShell process monitor as well as the POSIX timeout path.
- [x] Make the native Windows `cmd.exe` C17 preprocessing and language-boundary
      gates use the shared directory/expected-failure helpers, native tool names,
      and a committed PowerShell text matcher; i686 runtime execution remains
      explicitly dependent on a 32-bit host runtime or RinOS/WSL runner.
- [x] Emit file-scope data/BSS variable DIEs with source locations, type
      references, external-linkage flags, and `DW_OP_addr` relocations for
      external and internal-linkage symbols; verify x86/x64 `.ro` objects and
      linked `.rin` output in `test-debug-info`.
- [x] Emit `DW_AT_linkage_name` for file-scope variable DIEs so C++ source
      names retain their mangled ABI symbol identity in debug information.
- [x] Emit static-local variable DIEs as subprogram children with absolute
      symbol relocations, source locations, and collected types; verify the
      i686/AMD64 object and linked-image debug-info path.
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
      path.
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
