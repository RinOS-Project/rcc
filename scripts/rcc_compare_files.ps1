param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string] $First,
    [Parameter(Mandatory = $true, Position = 1)]
    [string] $Second
)

if (-not (Test-Path -LiteralPath $First -PathType Leaf) -or
    -not (Test-Path -LiteralPath $Second -PathType Leaf)) {
    exit 2
}

$left = [System.IO.File]::ReadAllBytes($First)
$right = [System.IO.File]::ReadAllBytes($Second)
if ($left.Length -ne $right.Length) {
    exit 1
}

for ($index = 0; $index -lt $left.Length; ++$index) {
    if ($left[$index] -ne $right[$index]) {
        exit 1
    }
}

exit 0
