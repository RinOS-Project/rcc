param(
    [Parameter(Mandatory = $true)] [string] $CompilerRoot,
    [Parameter(Mandatory = $true)] [string] $RinosRoot,
    [Parameter(Mandatory = $true)] [string] $TestOut
)

$ErrorActionPreference = 'Stop'
$CompilerRoot = (Resolve-Path -LiteralPath $CompilerRoot).Path
$RinosRoot = (Resolve-Path -LiteralPath $RinosRoot).Path
$TestOut = (Resolve-Path -LiteralPath $TestOut).Path

$vswhere = Join-Path ${env:ProgramFiles(x86)} `
    'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Visual Studio vswhere.exe is required for the i686 runtime test.'
}
$visualStudioRoot = & $vswhere -latest -products '*' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $visualStudioRoot) {
    throw 'Visual Studio x86 C/C++ tools are required for the i686 runtime test.'
}

$vcvarsall = Join-Path $visualStudioRoot 'VC\Auxiliary\Build\vcvarsall.bat'
$clang = Join-Path $visualStudioRoot 'VC\Tools\Llvm\bin\clang-cl.exe'
if (-not (Test-Path -LiteralPath $vcvarsall) -or
    -not (Test-Path -LiteralPath $clang)) {
    throw 'Install the Visual Studio x86 tools and LLVM clang-cl component.'
}

$libcRoot = Join-Path $RinosRoot 'public-base\libs\libc'
$testSource = Join-Path $CompilerRoot 'tests\c_complex_abi_test.c'
$emitSource = Join-Path $CompilerRoot 'src\emit_ro.c'
$utilsSource = Join-Path $CompilerRoot 'src\utils.c'
$runtimeSource = Join-Path $libcRoot 'complex_runtime.c'
$objectFile = Join-Path $TestOut 'complex-x86.ro'
$batchFile = Join-Path $TestOut 'run-complex-i686-msvc.bat'

if (-not (Test-Path -LiteralPath $objectFile)) {
    throw "Missing compiler-produced i686 object: $objectFile"
}

$batchLines = @(
    '@echo off'
    "call `"$vcvarsall`" x86 >NUL"
    'if errorlevel 1 exit /b 1'
    "set `"CLANG=$clang`""
    "set `"OUT=$TestOut`""
    "set `"ROOT=$RinosRoot`""
    '"%CLANG%" /nologo /std:c11 /clang:-frounding-math ' +
        '/D_CRT_SECURE_NO_WARNINGS ' +
        "/I`"%ROOT%\public-base\RinOS-SDK\include`" " +
        "/I`"$CompilerRoot\include`" /c " +
        "/Fo`"%OUT%\complex-abi-test-i686.obj`" " +
        "/I`"%ROOT%\public-base\toolchain\RinCompiler\include`" " +
        "/I`"$CompilerRoot\include`" `"$testSource`""
    'if errorlevel 1 exit /b 1'
    '"%CLANG%" /nologo /std:c11 /D_CRT_SECURE_NO_WARNINGS ' +
        "/I`"%ROOT%\public-base\RinOS-SDK\include`" " +
        "/I`"$CompilerRoot\include`" /c " +
        "/Fo`"%OUT%\emit-ro-i686.obj`" `"$emitSource`""
    'if errorlevel 1 exit /b 1'
    '"%CLANG%" /nologo /std:c11 /D_CRT_SECURE_NO_WARNINGS ' +
        "/I`"%ROOT%\public-base\RinOS-SDK\include`" " +
        "/I`"$CompilerRoot\include`" /c " +
        "/Fo`"%OUT%\utils-i686.obj`" `"$utilsSource`""
    'if errorlevel 1 exit /b 1'
    '"%CLANG%" /nologo /std:c11 /clang:-frounding-math ' +
        '/I"%ROOT%\public-base\RinOS-SDK\include" ' +
        "/I`"$libcRoot`" /D__rin_cmul_f32=rcc_host_complex_mul_f32 " +
        '/D__rin_cmul_f64=rcc_host_complex_mul_f64 ' +
        '/D__rin_cdiv_f32=rcc_host_complex_div_f32 ' +
        '/D__rin_cdiv_f64=rcc_host_complex_div_f64 /c ' +
        "/Fo`"%OUT%\complex-runtime-i686.obj`" `"$runtimeSource`""
    'if errorlevel 1 exit /b 1'
    '"%CLANG%" /nologo /D_CRT_SECURE_NO_WARNINGS ' +
        "/Fe:`"%OUT%\run-complex-i686.exe`" " +
        '"%OUT%\complex-abi-test-i686.obj" ' +
        '"%OUT%\emit-ro-i686.obj" "%OUT%\utils-i686.obj" ' +
        '"%OUT%\complex-runtime-i686.obj" /link /SUBSYSTEM:CONSOLE'
    'if errorlevel 1 exit /b 1'
    '"%OUT%\run-complex-i686.exe" "%OUT%\complex-x86.ro"'
    'if errorlevel 1 exit /b 1'
)

Set-Content -LiteralPath $batchFile -Value $batchLines -Encoding Ascii
try {
    & $batchFile
    if ($LASTEXITCODE -ne 0) {
        throw "The i686 runtime runner exited with code $LASTEXITCODE."
    }
}
finally {
    Remove-Item -LiteralPath $batchFile -Force -ErrorAction SilentlyContinue
}
