param(
    [Parameter(Mandatory = $true)]
    [string]$CompilerPath,
    [Parameter(Mandatory = $true)]
    [ValidateSet('i686-unknown-rinos', 'x86_64-unknown-rinos')]
    [string]$Target,
    [Parameter(Mandatory = $true)]
    [string]$OutputPath,
    [string]$SourcePath = 'tests/cxx_parser_recovery.cpp',
    [int]$TimeoutSeconds = 10
)

$ErrorActionPreference = 'Stop'
$compiler = (Resolve-Path -LiteralPath $CompilerPath).Path
$output = [System.IO.Path]::GetFullPath($OutputPath)
$directory = [System.IO.Path]::GetDirectoryName($output)
$stem = [System.IO.Path]::GetFileNameWithoutExtension($output)
$stdoutPath = Join-Path $directory ($stem + '.stdout')
$stderrPath = Join-Path $directory ($stem + '.stderr')
$logPath = [System.IO.Path]::ChangeExtension($output, '.log')

if (Test-Path -LiteralPath $output) {
    Remove-Item -LiteralPath $output -Force
}

$arguments = @(
    '--target', $Target,
    '-std=c++20',
    '-c',
    '-o', $output,
    $SourcePath
)
$quotedArguments = @($arguments | ForEach-Object { '"' + $_ + '"' })
$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $compiler
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
        throw "could not start RCC++ for target $Target"
    }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        try {
            $process.Kill()
            $process.WaitForExit()
        } catch {
            # Preserve the timeout as the primary failure even if the process
            # exits between the timeout check and Kill().
        }
        throw "RCC++ parser recovery timed out for $Target"
    }
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()
    [System.IO.File]::WriteAllText($stdoutPath, $stdout)
    [System.IO.File]::WriteAllText($stderrPath, $stderr)
    [System.IO.File]::WriteAllText($logPath, $stdout + $stderr)

    if ($process.ExitCode -ne 1) {
        throw "expected RCC++ exit code 1 for $Target, got $($process.ExitCode)"
    }
    $diagnostics = $stdout + $stderr
    if ($diagnostics.IndexOf('expected ;', [System.StringComparison]::Ordinal) -lt 0) {
        throw "expected parser diagnostic was not emitted for $Target"
    }
    if ($diagnostics.IndexOf('too many errors', [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
        throw "parser recovery hit the diagnostic limit for $Target"
    }
    if (Test-Path -LiteralPath $output) {
        throw "RCC++ left an object after rejecting the invalid $Target fixture"
    }
    Write-Output "C++ parser recovery rejected $Target with bounded diagnostics and no object"
} finally {
    $process.Dispose()
}
