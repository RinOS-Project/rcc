# rcc v3 実装状況

この一覧は「CLIが存在する」ことと「言語・ABIが完成している」ことを区別する。
未完項目が残る間はC17/C++20準拠やセルフホスト完了を宣言しない。

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
  register配置を追跡する完全なlocation list、inline attribution、任意prologueの
  CFIは未完了。
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
- [x] C17 conformance compile-and-run suite

## 3. C++20 frontend / ABI

- [x] `rcc++` entrypointとC++20既定mode
- [x] 明示的C++関数の`return`値有無・void式・変換可能性をsemaで検証し、
      不正なreference/scalar returnを両archで診断、void式returnをhost実行で検証
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
  - [ ] 標準C++の全conversion rank、ADL、two-phase lookup互換性
- [x] C++ enumのfixed underlying typeを保持し、signed/unsigned各幅の明示列挙子と
      暗黙の次値が表現可能か検査。i686/AMD64で境界値を実行し、範囲外を診断する。
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
  - [ ] full partial ordering, all parameter-pack deduction rules, and
        unsupported constexpr evaluation required for complete standard
        conformance
- [ ] C++20 modules、coroutines
- [ ] Complete ordinary non-template rvalue-reference binding and value-category
      semantics. Named xvalue local binding and alias-preserving `int&`/`int&&`
      function returns now have regressions that compile for both targets and
      execute on the x64 host. Source-level reference collapsing is covered;
      converted class xvalues, and broader call/return ABI interactions still
      need systematic coverage; a direct thread-local class-prvalue reference
      now has dedicated per-thread host coverage below.
  - [x] Reject non-template `int&` returns from prvalues and `int&&` returns
        from lvalues as hard C++ semantic errors; verify both cases, invalid
        reference initializers, and invalid ordinary calls for i686/AMD64.
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
  - [ ] Finish thread-local reference temporary acceptance. The i686/AMD64
        backends now emit per-thread owner/guard storage and register cleanup
        with `__cxa_thread_atexit`; libc start-return and `pthread_exit` paths
        run `__rin_cxa_thread_finalize` before `SYS_THREAD_EXIT`, ahead of POSIX
        TSD destructors. Host thread-exit regressions exist. Keep open until
        RinOS target execution and broader initializer/lifetime forms are
        verified.
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
        bindings through a member pointer verify retained values, lifetime, and
        exactly-once destruction through the shared host runtime. Keep this
        open until RinOS runtime integration is covered.
  - [ ] Complete remaining pointer-to-member conversions and contexts:
        pointer-to-member function types/calls, hidden inherited member lookup,
        non-public inherited members, and remaining access contexts. Unique
        public inherited data members now form pointers whose owner is the class
        that declared the member, including public virtual bases; positive
        generation/execution coverage and negative checks for private
        formation, ambiguous object paths, and ambiguous owner conversion run
        in `test-cxx-member-pointer-data` on both targets. Conversions across
        virtual bases are ill-formed under C++ `[conv.mem]`; preserve diagnostics
        for them instead of treating them as an implementation feature. Keep
        unsupported valid forms unchecked and explicit; do not substitute
        placeholder lowering.
  - [x] Preserve member-pointee `const` through same-owner and combined
        derived-owner conversions, reject qualification removal, and require an
        lvalue for built-in scalar assignment through an xvalue-selected member.
        Keep valid class xvalue copy assignment working; verify positive and
        negative cases on i686/AMD64 with `test-cxx-member-pointer-data` and
        compare the positive source against GCC C++20.
  - [x] Preserve xvalue category for a non-reference data member selected
        through an xvalue object; sema, `decltype(auto)`, and both i686/AMD64
        codegens agree. Reference data members remain lvalues. Cover reference
        returns and execution in `test-cxx-function-template-references`.
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
        the supported-type regressions and golden outputs pass. Local classes
        with member functions and local classes in function-template scopes
        remain explicitly unsupported in the separate unchecked item below.
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
    - [ ] Local classes inside function-template instantiations still need
          specialization-bound identities; the parser currently diagnoses
          this case instead of lowering it.
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
- [x] cleanup/VLA跨ぎを伴わないC goto/labelを事前収集したSSA CFG blockへ
      lowerし、i686/AMD64 verified backend emit回帰を追加。cleanup/VLA跨ぎは
      引き続き明示的にverified subset外として扱う。
- [ ] aggregate/vector/exceptionを含む全frontendのtyped SSA lowering
  - [x] `switch`の`case`ラベルを`while`/`do`/`for`本体内から収集し、通常のswitch
        dispatchとloop backedgeを保ったverified SSA CFGへlowerする。i686/AMD64
        `-O2` emissionとx86_64通常／最適化後実行を回帰検証
  - [x] `switch`内のgoto label配下にある`case`を収集し、前段caseからのgotoと
        switch直dispatchを同じlabel/case CFGへ合流させる。両archの通常/O2で
        fallbackなし、x86_64で両経路を実行検証
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
        execution in `test-verified-cxx-temporary-cleanup`. Potentially throwing
        calls and array-loop cleanup plans remain on the complete backend.
  - [x] Lower same-type noexcept class-prvalue conditional arguments bound to
        reference parameters. Pass the final lifetime-owner storage directly
        to the selected branch's sret call or capture a one-/two-register
        aggregate return directly into that storage; run the validated
        fixed-member destructor plan once after the outer call. Verify both
        branch choices, cleanup order, fallback-free i686/AMD64 objects, and x64
        execution in `test-verified-cxx-temporary-cleanup`. Member-inline calls,
        conversions, nested conditional arms, potentially throwing expressions,
        and array-loop cleanup remain outside this verified subset.
  - [x] Validate noexcept fixed-member cleanup plans recursively for class
        prvalue reference arguments of a selected conditional branch call;
        emit the inner temporary cleanup immediately after that call and the
        conditional result cleanup after its consumer. Verify both branch
        choices, inner/outer cleanup order, fallback-free i686/AMD64 objects,
        and x64 execution in `test-verified-cxx-temporary-cleanup`.
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
  - [x] typed-SSAで同一整数値の`x - x`／`x ^ x`を0へ、`x & x`／`x | x`を
        元の値へ変形し、8/16/32/64-bitのverifier回帰で値置換と定数化を検証
  - [x] typed-SSAの符号付き／符号なし`x % 1`を0へ定数化し、除数0などの
        未定義・条件依存ケースを保持したまま8/16/32/64-bitで検証
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
  - [x] 同一block内の同一SSA address・同一型のnon-volatile loadをCSEし、
        互いに別のdirect allocaと証明できるstoreだけを跨ぎ、それ以外の
        store/call/volatile access、型違い、block境界では再利用しないことを
        IR verifier regressionで検証
  - [x] 同一block内でdirect allocaへの直近store値を同じ型のnon-volatile loadへ
        forwardingし、unknown alias store/call/volatile loadでは全事実、
        volatile storeでは対象slotの値事実を失効させるIR verifier regressionを追加
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
  - [x] C17の`while`／`do-while`で`i += 2`／`i -= 2`／`i = i + 2`のような
        ±4以内の定数strideを同じoverflow・方向・`!=`到達性proofへ接続し、
        1〜4回のi686/AMD64展開と実行を回帰検証
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
  - [x] C17 `for`の`i += 2`／`i -= 2`／`i = i + 2`形式を±4以内のbounded
        constant-stride proofへ接続し、符号付き／符号なしoverflow・方向・
        `!=`到達性を保持した1〜4-trip展開と両archの実行回帰を追加
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
  - [ ] typed SSAのi686 scalar floating arithmeticをx87 register-stack規約と
        cdecl引数／戻り値ABIに合わせて実装し、C/C++の`-O0`/`-O2`を実機以外で
        fallbackなし実行検証する。
    - [x] x87 typed arithmeticが未実装のi686ではfloat/double型をlowering前に
          legacy backendへ戻し、最適化で定数式になった関数がx86 selectorで
          late failureしないことをC/C++ `test-optimize`で検証。
  - [x] i686 wide-scalarの代入、複合代入、pre/post incrementをpair
        load/storeとcarry/borrow付き演算へlowerし、両archのobject・x64
        実行回帰で検証
  - [x] i686 wide-scalarのtruth、論理否定、short-circuit AND/ORを既存の
        two-word truth reductionとCFGへ接続し、両archのobject・x64実行で検証
  - [x] wide-scalar戻り値を持つdirect variadic callでi686のinteger/pointer
        引数をSSA loweringし、narrow integerのC default promotionと64-bit
        引数のlow/high配置を含むi686/x64 object・外部call relocationを検証。
        floating-point/aggregate variadic引数とx86_64 SysV `va_arg`関数本体は対象外
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
        1 GPRだけ空いた境界では分割せず、専用legacy-object fixtureへ分離して
        `-O0`/`-O2`で実際のcomplete backendの結果を実行確認。
      - [x] single SSE eightbyteに分類される単一`float`／`double` field aggregateを
        variadic call-siteからmarshalし、C/C++ objectでXMM register、最後のXMM register、
        SSE register枯渇後のstack配置を`-O0`/`-O2`実行検証。
      - [x] 2個のSSE eightbyteを持つtrivial aggregateを両XMM registerへ渡す経路と、
        SSE bank枯渇後に両方stackへ置く経路をC/C++で実行検証する。残り1 XMM slotでは
        分割せず専用legacy-object fixtureへ分離し、`-O0`/`-O2`で実行確認する。
        C/C++の`-O0`/`-O2`でfallbackなしの両XMM／全stack経路を実行し、
        one-XMM-slot straddleは専用legacy-only fixtureのcomplete backend結果を実行確認。
      - [x] 最大2個のINTEGER/SSE eightbyteで構成されるmixed GP/SSE aggregateを、
        必要な両register bankに余裕がある場合と両bank枯渇後のstack配置でmarshalする。
        一方のbankだけが不足する境界ではaggregate全体を分割せずcomplete legacy objectへ
        fallbackし、C/C++の`-O0`/`-O2`でregister／全stack経路とlegacy境界経路を
        runtime検証。
      - [x] nonvariadic x86_64 SysVのnamed trivial aggregate parameterをeightbyteごとの
        INTEGER/SSE classでfunction entryとcall-site双方に分類する。C/C++の`-O0`/`-O2`で
        register配置と全体stack配置を実行検証し、typed ABIで未対応のregister straddleは
        complete legacy-object fallbackで実行確認。mixed GP/SSEのregister配置と両bank
        exhaustion後のstack配置をfallbackなしで実行し、片bankだけの境界はlegacy objectで確認。
      - [x] variadic functionのnamed trivial aggregate parameterについて、GP/XMM
        register配置と両bank exhaustion時のwhole-stack配置をfunction entry、call-site、
        `va_start` overflow位置まで接続。C/C++の`-O0`/`-O2`でregister／stack後の
        `va_arg(int/double)`をruntime検証し、fallbackなしで実行。
      - [x] variadic call-siteから24-byte MEMORY-class aggregateをtyped SSAで
        by-value stack marshalし、C/C++の`-O0`/`-O2`でcalleeの`va_arg`結果を
        fallbackなしでruntime検証する。
      - [x] 16-byte aligned 32-byte C++ aggregateを9個目のstack double後に
        `va_arg`し、さらに24-byte MEMORY aggregateを続けて読む経路をtyped SSAで
        lowerし、call loweringによる複数のMEMORY descriptor stack配置とcalleeの
        overflow cursor/alignment更新をfallbackなしで接続する。9個のdoubleを消費した後の
        aggregate fieldと後続aggregate fieldの値をC++ `-O0`/`-O2` runtimeで検証する。
        unsupported floating-point binary arithmeticをfixtureから分離し、対象関数自身が
        typed SSAで出力されることをfallback reason検査でも固定する。
        現在のsource progress: 対応するinteger/pointer/float/double aggregateの`va_arg`は、
        INTEGER/SSE eightbyte分類、register-save／stack fallback、16-byte stack alignment、
        16-byteを超えるMEMORY aggregateを実装・実行検証した。variadic callerは最大2 INTEGER
        eightbyte、最大2個のhomogeneous SSE eightbyte、または最大2個のmixed INTEGER/SSE
        eightbyteを持つtrivial aggregateに限定される。nonvariadic named aggregate parametersも同じ
        bounded INTEGER/SSE分類でregister／whole-stack loweringと実行検証を完了し、variadic
        functionのbounded named aggregateもregister／whole-stack配置と`va_start` cursorを
        C/C++ O0/O2で実行検証した。片bankだけのregister straddleはcomplete legacy backendで
        実行する。alignmentが16-byteを超えるcaller、
        bounded profile外のnamed aggregate layouts、残るadjusted-`va_list`形態は未実装であり、
        このparent checkboxは未完了のままにする。
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
- [x] SysV aggregate分類、floating-point/aggregate variadic ABI
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
  - [x] C++ reference declaratorsを保持し、`DW_TAG_reference_type`／
        `DW_TAG_rvalue_reference_type` DIEから参照先型への`DW_AT_type`を
        出力。lvalue/rvalue reference parameterのDIEと参照先base typeを
        i686/AMD64の`test-debug-info`で検証
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
        から除外する回帰を`test-debug-info`へ接続。命令rangeを持たない宣言、
        全制御フローでの正確なlifetime、register/piece locationsは未完
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
  - [ ] `.debug_info`のinline attribution／全scopeの宣言開始・終了に一致する
        location lists、register・piece location追跡、非標準prologueを含む
        完全なCFI/unwind
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
  - [x] GCC `g` input/output constraintは有効なGPR alternativeを決定的に
        選び、`=`／`+` outputの保存、input評価、衝突検査を通してlowerする。
        Cでi686/AMD64 byte・生成検証とx64実行、C++でx64生成byteを検証し、
        `test-inline-asm-execute`をproduction `test-ci`へ追加
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
- [x] CI workflowでfull `test-ci` production gateをGCC/Clang双方に設定し、
      C17/C++20、IR/MIR、verified backend、optimizer、ABI、image、bootstrapを
      matrix実行。Makefileの環境変数／command-line `CC`保持と、built-in `cc`
      のみの場合のGCC defaultを回帰検証
- [x] GCC full `test-ci` production gateでC17/C++20、IR/MIR、verified backend、
      optimizer、ABI、image、bootstrap全経路の成功を確認し記録する
  - [x] GCC: `make -j1 OBJDIR=build/wsl-gcc/obj
        BINDIR=build/wsl-gcc/bin TEST_OUT=build/wsl-gcc/tests CC=gcc test-ci`
        passed on the Linux/WSL host, including both target architectures.
- [ ] Clang full `test-ci` production gateを実行し、同じ全経路の成功を記録する
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
        C17/C++20の有効変異を両archで再現コンパイルし、無効変異のsignal／
        timeout／空診断／誤った成果物を拒否する
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
