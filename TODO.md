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
  - [ ] verified backendで複数scalar member/parameterやより一般的なconstructor
        body、nontrivial base initialization、nontrivial/deleted target copy
        constructor、call引数以外のfull-expression cleanup、例外経路を実装・検証。
        標準conversion subsequenceとpointer-to-memberを含む全rank/partial orderingも
        未完了
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
      - [x] Add i686/AMD64 runtime coverage for fixed-size class-array members
            initialized through inherited constructors and ordinary constructor
            prologues, including empty-brace array default member initializers;
            assert per-element construction order and retain negative coverage
            for unsupported nested arrays and inherited element cleanup. `make SHELL=cmd.exe
            test-cxx-inherited-constructor` passes freestanding i686 execution,
            AMD64 host execution, and object generation for both target widths.
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
      throwing/partial-construction cleanupは引き続きsubset外。
- [ ] aggregate/vector/exceptionを含む全frontendのtyped SSA lowering
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
  - [ ] Throwing argument/initializer evaluation before a cleanup owner is
        fully constructed, constructor-driven partial construction and unwind,
        potentially throwing cleanup callbacks, VLA forms outside the currently
        verified subset (and VLA interaction with throwing cleanup), and other
        unvalidated automatic cleanup-plan shapes remain outside this subset.
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
  - [x] typed SSAの符号付き`SDIV`／`SREM`も定数±2冪をsign-fill biasと
        arithmetic shiftでlowerし、全width 8/16/32/64のIR interpreterで
        8-bit全入力、境界値と4096 deterministic inputsを照合。0、-1、非2冪は
        保持し、`INT_MIN` divisorを含むIR verifier回帰を追加
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
        その他のalignment値は網羅していない。
        bounded profile外のnamed aggregate layouts、4096 byteを超えるalignment、および
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
  - [x] legacy i686/AMD64のinlined-subroutine DIEに、callee本体直下のautomatic
        const scalar localをabstract declarationとconcrete instanceに分けて出力。
        signed/unsigned integer・enum・float/doubleの保存済み定数値をDWARFへ記録し、
        定数initializerが最適化で消えた場合も再構成してpure-scalar inlineを維持する。
        `test-debug-info`でconst type、abstract-origin、source file、signed値7を両archで検証し、
        `test-optimize`も成功。nested lexical localやlocation-list値は未対応
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
        Remaining work includes arbitrary nonconstant argument expressions,
        mutable parameters, optimized-away/register-only sources, nested lexical
        locals, tracking optimized variables across register and piece
        locations, declaration lifetimes across every optimized CFG edge, and
        CFI/unwind coverage for prologues and epilogues beyond current fixed-
        frame rules. The checked entries distinguish these bounded cases from
        the still-incomplete general variable-location implementation.
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
        instruction bytes, reject global/indirect operands and AMD64 use, and
        include i686 execution in the POSIX inline-asm execution gate.
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
- [ ] Complete member-template deduction and overload selection for remaining
  general dependent non-type expressions and template-template forms,
  expansions involving
  multiple distinct packs and other nested patterns, and remaining overload
  edge cases; support
  constructor templates, explicit member-template specializations, inherited
  conversion templates, full C++20 constraint subsumption, and complex
  dependent Itanium ABI substitutions. Conversion target deduction and
  user-defined sequence ranking are specified in
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
- [ ] Extend deeper dependent-owner/cross-kind pack combinations. Structural
  non-integral NTTP packs remain open.

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
- [ ] Expand deeper dependent-owner/cross-kind pack coverage. Defaults and
  constraints in nested signatures and structural non-integral NTTP packs
  remain open.

## RCC continuation (2026-10-14, alias-template owners of nested aliases)

- [x] Resolve an alias-template specialization used as the owner of a nested
  alias template, then validate its template-template pack arguments. Cover
  type and integral non-type owner parameters, and class/alias template actuals.
- [x] Pass the focused native-Windows `test-cxx-alias-templates` gate on
  i686/x86_64, including x86_64 execution and GCC C++20
  `-Wall -Wextra -Werror` execution.
- [x] Pass the full native-Windows MinGW GCC `test-cxx` and `test-debug-info`
  aggregates after this parser change (both exit 0).
- [ ] Forward dependent template-template packs through nested alias-template
  specializations and support deeper multi-source-pack expansion.
- [ ] Add explicit/partial nested member specializations for dependent owners
  and outer class-template-specialization owner patterns. Structural
  non-integral owner packs and deeper cross-kind dependent-owner expansion also
  remain open.
- [ ] Extend structural non-integral NTTP packs and defaults/constraints in
  nested template-template signatures.
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
- [ ] Add nested member specializations owned by outer class-template
  specializations, including structural NTTP owner arguments and dependent
  owner declarations.

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
- [ ] Add explicit/partial nested member specializations for dependent or outer
  class-template-specialization owners, plus structural NTTP owner packs.

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
- [ ] Non-finite values, `long double`, extended precision/rounding cases,
      and broader floating constant-expression conversions remain open.

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
- [ ] Member-pointer arrays and multidimensional arrays of structural-class
      elements remain open.

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
- [ ] Gotos inside structured wide-scalar loop lowering and full SSA
      predecessor-phi construction at arbitrary labels remain open.

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
- [ ] General typed-SSA VLA lowering remains incomplete for aggregate
      initialization/copy forms, atomic objects, C++ class lifetime hooks,
      and broader cleanup/unwind plus goto/loop-edge combinations.

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
- [ ] VLA aggregate initialization/copy, atomic elements, C++ class lifetime
      lowering, and cleanup/unwind/control-flow interactions remain open.

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
- [ ] Aggregate initialization/copy, atomic VLA forms, plus broader
      cleanup/unwind and control-flow interactions, remain open.

## RCC continuation (2026-10-20, volatile scalar typed-SSA VLA elements)

- [x] Permit volatile scalar VLA elements through typed SSA and preserve their
      observable load/store accesses; keep atomic and volatile aggregate forms
      outside this supported subset.
- [x] Add a runtime regression that writes and reads volatile elements at both
      ends of a VLA. The verified-backend i686/AMD64 `-O0`/`-O2` gates emit all
      28 functions with zero fallbacks and x64 execution passes. IR/lowering,
      classic VLA-runtime, and dual-architecture stage0-to-stage2 bootstrap
      gates pass. RCC `main` implementation commit: `8f67474`. No push.
- [ ] VLA aggregate initialization/copy, atomic objects, C++ class lifetime,
      and cleanup/unwind/control-flow interactions remain open.

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
- [ ] Explicit/non-trivial intermediate virtual-base destructor graphs,
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
      outside call arguments, and exception/unwind interactions remain open.

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
- [ ] Virtual/inherited bases, potentially-throwing base constructors and
      partial-construction unwind, general constructor bodies, and
      full-expression cleanup outside call arguments remain open. Trivial
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
- [ ] Long-double conversions, nonconstant floating expressions, dynamic
      pointer values, symbolic addresses outside the supported direct
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
- [ ] Additional C++ enum conversion cases, nonliteral enum-value attribution,
      nonconstant parameter locations, and optimized register/piece tracking
      remain open.

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
- [ ] Register-only or optimized-away sources, source/destination type
      conversions, compound dynamic expressions, mutable-parameter tracking,
      and optimized register/piece tracking remain open; this does not
      implement general variable-location tracking.

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
- [ ] General pointer arithmetic, nested member/array subobject paths, pointer
      casts requiring target adjustment, dynamic pointer expressions, and
      general register/piece location tracking remain open.

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
- [ ] Multidimensional/nested subarray paths such as `&matrix[i][j] ± N`,
      negative displacements not rooted at a statically indexed array element,
      target-adjusting casts, dynamic pointer expressions, and optimized
      register/piece location tracking remain open.
