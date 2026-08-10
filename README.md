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

秘密鍵をcompilerや成果物へ埋め込む経路はありません。最終出力には
`--rinsign`、`--sign-key`、`--public-key`を明示します。
`--emit-unsigned-v3`はlinker/validatorの試験専用です。

## 実装済みの基盤

- 独自lexer/parser/semaとi386/AMD64 code generator
- `-E`, `-S`, `-c`, `-shared`, `-driver`, `-MMD`, `-MF`
- `-I`, `-D`, `-U`, `-nostdinc`, `-ffreestanding`
- 両target tripleと矛盾する`-m32/-m64`指定の拒否
- nested include、function-like/variadic macro、条件付きpreprocess
- C17 `_Static_assert`の整数定数式評価と失敗diagnostic
- x86_64 SysVの整数引数、基本scalar/aggregate load-store、global data
- `.ro/.ra v2` reader/writer、typed import、依存libraryを扱う`rld`
- external signerを安全な引数配列で起動する最終v3出力

`-O0`から`-O3`および`-g`のCLIは予約済みですが、各levelのSSA最適化と
完全なDWARF生成は未完成です。C++ frontendも実験段階で、classの基本構文を
越えるtemplates、exceptions、RTTI、modules、coroutines等は完成していません。

## ビルド

POSIX環境では次でhost toolchainを作成します。

```sh
make -j
```

生成物は`rcc`、`rcc++`、`rld`、`rar`です。Windows用既存binaryを更新せずに
検証する場合は、出力directoryを分離できます。

```sh
mkdir -p ../build/rcc/obj ../build/rcc/bin
make -j OBJDIR="$PWD/../build/rcc/obj" BINDIR="$PWD/../build/rcc/bin"
```

## 使用例

```sh
rcc --target i686-unknown-rinos -c -MMD -MF app.d -o app.ro app.c
rcc --target x86_64-unknown-rinos -S -o app.s app.c
rar r libsample.ra sample.ro
rld --target x86_64-unknown-rinos --shared \
  --dep rinbase.rll --import rin_log_write=rinbase.rll@function \
  --rinsign ../scripts/rinsign.py --sign-key debug.pem \
  --public-key debug-public.der -o sample.rll sample.ro
```

## 回帰試験

```sh
make test-static-assert
make test-cxx-cli
make test-link
make test-archive
```

RinOS親repositoryには、preprocessor、x86_64実行ABI、SDK v1 packaging、
production RIN v3 validatorを組み合わせた統合試験もあります。

## 完成条件

公開toolchainとしての完成条件は、C17、主要C++20、typed SSA/MIRと最適化、
i386/AMD64 ABI、DWARF unwind、PIC/PIE、TLS/exception/RTTI、stage2再現build、
RinOS上の32/64-bitセルフホストです。進捗は[`TODO.md`](TODO.md)を参照してください。
