param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $Arguments
)

$fixed = $false
$quiet = $false
$wholeLine = $false
$count = $false
$pattern = $null
$paths = @()

foreach ($argument in $Arguments) {
    if ($null -eq $argument) { continue }
    if ($argument -eq '-F') { $fixed = $true; continue }
    if ($argument -eq '-q') { $quiet = $true; continue }
    if ($argument -eq '-x') { $wholeLine = $true; continue }
    if ($argument -eq '-c') { $count = $true; continue }
    if ($null -eq $pattern) {
        $pattern = $argument.Trim([char[]]@("'", '"'))
    } else {
        $paths += $argument.Trim([char[]]@("'", '"'))
    }
}

if ($null -eq $pattern) {
    exit 2
}

$lines = @()
if ($paths.Count -gt 0) {
    foreach ($path in $paths) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            exit 2
        }
        $lines += Get-Content -LiteralPath $path
    }
} else {
    $lines = @($input)
}

$matches = @($lines | Where-Object {
    $line = [string]$_
    if ($wholeLine) { return $line -ceq $pattern }
    if ($fixed) { return $line.IndexOf($pattern, [System.StringComparison]::Ordinal) -ge 0 }
    return $line -match $pattern
})

if ($count) {
    $matches.Count
} elseif (-not $quiet) {
    $matches | ForEach-Object { $_ }
}

if ($matches.Count -gt 0) { exit 0 }
exit 1
