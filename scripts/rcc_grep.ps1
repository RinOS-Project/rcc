param(
    [Alias('F')]
    [switch] $Fixed,
    [Alias('q')]
    [switch] $Quiet,
    [Alias('x')]
    [switch] $WholeLine,
    [Alias('c')]
    [switch] $Count,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $Arguments
)

$fixed = $Fixed.IsPresent
$quiet = $Quiet.IsPresent
$wholeLine = $WholeLine.IsPresent
$count = $Count.IsPresent
$pattern = $null
$paths = @()

# Windows make recipes run under cmd.exe, which does not group single-quoted
# arguments. Reassemble a single-quoted fixed string before interpreting the
# pattern/path boundary; PowerShell and POSIX shells already pass it as one
# argument, so this is a no-op there.
$normalizedArguments = [System.Collections.Generic.List[string]]::new()
for ($index = 0; $index -lt $Arguments.Count; $index++) {
    $argument = [string]$Arguments[$index]
    if (-not $argument.StartsWith("'")) {
        $normalizedArguments.Add($argument)
        continue
    }

    $parts = [System.Collections.Generic.List[string]]::new()
    $part = $argument.Substring(1)
    if ($part.EndsWith("'")) {
        $parts.Add($part.Substring(0, $part.Length - 1))
        $normalizedArguments.Add(($parts -join ' '))
        continue
    }
    $parts.Add($part)
    $closed = $false
    while ($index + 1 -lt $Arguments.Count) {
        $index++
        $part = [string]$Arguments[$index]
        if ($part.EndsWith("'")) {
            $parts.Add($part.Substring(0, $part.Length - 1))
            $closed = $true
            break
        }
        $parts.Add($part)
    }
    if (-not $closed) { exit 2 }
    $normalizedArguments.Add(($parts -join ' '))
}

foreach ($argument in $normalizedArguments) {
    if ($null -eq $argument) { continue }
    if ($argument -eq '--') { continue }
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
