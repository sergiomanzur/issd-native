$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
cmake -S $Root -B (Join-Path $Root 'build') -G Ninja -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build (Join-Path $Root 'build') --config Release --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host ''
Write-Host 'Built the generated-code static library.'
Write-Host 'No playable executable was produced; see README.md under "Continue the port".'
exit 0
