param(
    [Parameter(Mandatory = $true)][string]$QtRoot,
    [string]$TorBinary = ""
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$qtDirectory = (Resolve-Path -LiteralPath $QtRoot).Path
$originalPath = $env:Path
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmakeExecutable = if ($cmakeCommand) { $cmakeCommand.Source } else { 'C:\Program Files\CMake\bin\cmake.exe' }
function Check-Exit([string]$step) {
    if ($LASTEXITCODE -ne 0) { throw "$step failed (exit $LASTEXITCODE)" }
}
Push-Location $projectRoot
try {
    $env:Path = (Join-Path $qtDirectory 'bin') + ';' + $env:Path
    & $cmakeExecutable -S src -B build-group -DCMAKE_BUILD_TYPE=Release -DFORCE_QT5=ON "-DCMAKE_PREFIX_PATH=$qtDirectory" -DENABLE_TESTS=OFF
    Check-Exit 'Configure application'
    & $cmakeExecutable --build build-group --config Release --target ricochet-refresh-qt --parallel 4
    Check-Exit 'Build application'
    & $cmakeExecutable -S tests/groups -B build-group-tests "-DCMAKE_PREFIX_PATH=$qtDirectory"
    Check-Exit 'Configure tests'
    & $cmakeExecutable --build build-group-tests --config Release --parallel 4
    Check-Exit 'Build tests'
    & (Join-Path (Split-Path $cmakeExecutable) 'ctest.exe') --test-dir build-group-tests -C Release --output-on-failure
    Check-Exit 'Tests'
    $destination = Join-Path $projectRoot 'dist\group-ricochet'
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    Copy-Item -LiteralPath 'build-group\ricochet-refresh-qt\Release\group-ricochet.exe' -Destination $destination
    Copy-Item -LiteralPath 'GROUP_RICOCHET.md','LICENSE' -Destination $destination
    & (Join-Path $qtDirectory 'bin\windeployqt.exe') --release --no-translations --qmldir src/ricochet-refresh-qt/ui (Join-Path $destination 'group-ricochet.exe')
    Check-Exit 'Deploy Qt'
    if ($TorBinary) {
        Copy-Item -LiteralPath (Resolve-Path -LiteralPath $TorBinary).Path -Destination (Join-Path $destination 'tor.exe')
    }
    Write-Output (Join-Path $destination 'group-ricochet.exe')
} finally {
    $env:Path = $originalPath
    Pop-Location
}
