# Use the rendering mode verified by the local UI and portable smoke tests.
$env:QT_QUICK_BACKEND = 'software'
$projectRoot = Split-Path $PSScriptRoot -Parent
$previewExe = Join-Path $projectRoot 'dist\group-ricochet\group-ricochet.exe'
if (!(Test-Path -LiteralPath $previewExe)) { throw 'Build the desktop preview first.' }
& $previewExe @args
