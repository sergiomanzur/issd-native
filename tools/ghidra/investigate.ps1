param(
    [Parameter(Mandatory=$true)][string]$GhidraHome,
    [Parameter(Mandatory=$true)][string]$JavaHome,
    [string[]]$Ranges = @(
        '81BD58:81BE67:data',
        '848BF5:848C52', '84C189:84C196', '84BF92:84C006',
        '84BD27:84BD77', '84E58F:84E5AA', '84E5D1:84E6AB', '84E6AC:84E706',
        '82DA46:82DA75:data', '82E3E4:82E413:data',
        '80BF05:80BF2B', '80BF76:80BFAD', '80C08F:80C0DA',
        '80C115:80C14E', '80BEEC:80BF01', '80C325:80C339:m1x1', '829D17:829D72:data',
        '85B404:85B430',
        '85D32E:85D350', '85D351:85D36B:data', '85D36C:85D38E',
        '8B948B:8B950B', '8BC8C0:8BC8CC', '8BC8CD:8BC8E6:data', '8BC8E7:8BC901'
    ),
    [string]$OutputName = 'rom-evidence.txt'
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$work = Join-Path $repo 'build/ghidra-investigation'
$module = Join-Path $work 'extension/ghidra-snes'
$headless = Join-Path $GhidraHome 'support/analyzeHeadless.bat'
if (!(Test-Path -LiteralPath $headless)) { throw 'Ghidra headless launcher missing' }
if (!(Test-Path -LiteralPath (Join-Path $JavaHome 'bin/java.exe'))) { throw 'Java executable missing' }
if (!(Test-Path -LiteralPath (Join-Path $module 'lib/ghidra-snes.jar'))) {
    throw 'Extract ghidra-snes v1.3.0 into build/ghidra-investigation/extension first; see README.'
}
# v1.3.0's manifest contains two unsupported lines. Correct only this local copy.
Set-Content -LiteralPath (Join-Path $module 'Module.manifest') -Value 'MODULE NAME: Ghidra-SNES'
$rom = Join-Path $repo 'International Superstar Soccer Deluxe (USA).sfc'
$expected = 'd2fe66c1ce66c65ce14e478c94be2e616f9e2cad374b5783a6a64d3c1a99cfa9'
if ((Get-FileHash -LiteralPath $rom -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) {
    throw 'Unsupported ROM hash'
}
$oldJava = $env:JAVA_HOME
$oldOptions = $env:GHIDRA_HEADLESS_JAVA_OPTIONS
$temporaryOutput = Join-Path $work ("export-" + [guid]::NewGuid().ToString('N') + '.txt')
try {
    $env:JAVA_HOME = $JavaHome
    $env:GHIDRA_HEADLESS_JAVA_OPTIONS = "-Dghidra.external.modules=`"$module`""
    # A simple basename avoids the supplied Windows launcher's import quoting bug.
    $stagedRom = Join-Path $work 'issd.sfc'
    if (!(Test-Path -LiteralPath (Join-Path $work 'ISSD.gpr'))) {
        Copy-Item -LiteralPath $rom -Destination $stagedRom
        & $headless $work ISSD -import $stagedRom -noanalysis -log (Join-Path $work 'import.log')
        if ($LASTEXITCODE -ne 0) { throw 'Ghidra import failed' }
    }
    & $headless $work ISSD -process issd.sfc -noanalysis -scriptPath $PSScriptRoot `
        -postScript IssdInvestigate.java `
        (Join-Path $repo 'deps/ISSD-disassembly/International_Superstar_Soccer_Deluxe/Routine_Macros_ISSD.asm') `
        $temporaryOutput @Ranges -log (Join-Path $work 'analysis.log')
    if ($LASTEXITCODE -ne 0) { throw 'Ghidra script failed; inspect analysis.log' }
    if (!(Test-Path -LiteralPath $temporaryOutput)) { throw 'Evidence output missing' }
    if ((Get-Content -LiteralPath $temporaryOutput -Tail 1) -ne 'EXPORT_COMPLETE') {
        throw 'Evidence export incomplete; inspect analysis.log'
    }
    Move-Item -LiteralPath $temporaryOutput -Destination (Join-Path $work $OutputName) -Force
} finally {
    $env:JAVA_HOME = $oldJava
    $env:GHIDRA_HEADLESS_JAVA_OPTIONS = $oldOptions
}
