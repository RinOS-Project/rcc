param(
    [Parameter(Mandatory = $true)]
    [string]$RccPath,
    [Parameter(Mandatory = $true)]
    [string]$CxxPath,
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,
    [string]$SourcePath = 'tests/cxx_union_member_invalid.cpp',
    [int]$TimeoutSeconds = 20
)

$ErrorActionPreference = 'Stop'
$outputRoot = [System.IO.Path]::GetFullPath($OutputDirectory)
[System.IO.Directory]::CreateDirectory($outputRoot) | Out-Null

function Resolve-Executable([string]$Path) {
    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        return (Resolve-Path -LiteralPath $Path).Path
    }
    $command = Get-Command -Name $Path -CommandType Application -ErrorAction Stop
    return $command.Source
}

function Invoke-Compiler([string]$Executable, [string[]]$Arguments) {
    $quotedArguments = @($Arguments | ForEach-Object {
        '"' + $_.Replace('"', '\"') + '"'
    })
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $Executable
    $startInfo.Arguments = $quotedArguments -join ' '
    $startInfo.WorkingDirectory = (Get-Location).Path
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.CreateNoWindow = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    try {
        if (-not $process.Start()) {
            throw "could not start $Executable"
        }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            try {
                $process.Kill()
                $process.WaitForExit()
            } catch {
                # Keep timeout failure as the primary result.
            }
            throw "$Executable timed out"
        }
        return @{
            ExitCode = $process.ExitCode
            Output = $stdoutTask.GetAwaiter().GetResult() +
                     $stderrTask.GetAwaiter().GetResult()
        }
    } finally {
        $process.Dispose()
    }
}

$rcc = Resolve-Executable $RccPath
$cxx = Resolve-Executable $CxxPath
$expectedDiagnostics = @(
    'a union data member cannot have reference type',
    'a union member function cannot be virtual or override a base member',
    'a union cannot have base classes',
    'a union may have only one default member initializer'
)

$gccResult = Invoke-Compiler $cxx @(
    '-std=c++20', '-pedantic-errors', '-fsyntax-only', $SourcePath
)
[System.IO.File]::WriteAllText(
    (Join-Path $outputRoot 'invalid-gcc.log'), $gccResult.Output)
if ($gccResult.ExitCode -eq 0) {
    throw 'GCC unexpectedly accepted the invalid union fixture'
}

foreach ($target in @('i686-unknown-rinos', 'x86_64-unknown-rinos')) {
    $stem = if ($target -eq 'i686-unknown-rinos') { 'invalid-x86' } else { 'invalid-x64' }
    $objectPath = Join-Path $outputRoot ($stem + '.ro')
    $logPath = Join-Path $outputRoot ($stem + '.log')
    if (Test-Path -LiteralPath $objectPath) {
        Remove-Item -LiteralPath $objectPath -Force
    }
    $result = Invoke-Compiler $rcc @(
        '--target', $target, '-std=c++20', '-c',
        '-o', $objectPath, $SourcePath
    )
    [System.IO.File]::WriteAllText($logPath, $result.Output)
    if ($result.ExitCode -ne 1) {
        throw "expected RCC++ exit code 1 for $target, got $($result.ExitCode)"
    }
    foreach ($diagnostic in $expectedDiagnostics) {
        if ($result.Output.IndexOf(
                $diagnostic, [System.StringComparison]::Ordinal) -lt 0) {
            throw "RCC++ did not report '$diagnostic' for $target"
        }
    }
    if (Test-Path -LiteralPath $objectPath) {
        throw "RCC++ left an object after rejecting the invalid $target fixture"
    }
}

Write-Output 'RCC++ and GCC reject invalid C++ union members on both targets'
