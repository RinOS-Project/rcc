param(
    [Parameter(Mandatory = $true, Position = 0)]
    [int] $Expected,
    [Parameter(Mandatory = $true, Position = 1)]
    [string] $Pattern,
    [Parameter(Mandatory = $true, Position = 2)]
    [string] $Path
)

if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
    exit 2
}

$count = 0
foreach ($line in (Get-Content -LiteralPath $Path)) {
    if ([string]$line -and
        $line.IndexOf($Pattern, [System.StringComparison]::Ordinal) -ge 0) {
        ++$count
    }
}

if ($count -eq $Expected) {
    exit 0
}
exit 1
