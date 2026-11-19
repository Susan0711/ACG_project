# Windows/MSVC handoff entry point. Not executed on Windows by the authoring agent.
param([string]$Repository = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
$Repository = (Resolve-Path $Repository).Path
$Build = Join-Path $Repository 'build-msvc'
$Assets = Join-Path $Repository 'assets'
function Run-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
Run-Checked 'cmake' @('-S', $Repository, '-B', $Build, '-G', 'Visual Studio 17 2022', '-A', 'x64')
Run-Checked 'cmake' @('--build', $Build, '--config', 'Release')
Run-Checked 'ctest' @('--test-dir', $Build, '-C', 'Release', '--output-on-failure')
$Output = Join-Path $Build ('validation-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $Output | Out-Null
$Executable = Join-Path $Build 'Release/path_tracer.exe'
Push-Location $Build
try {
    foreach ($Threads in @(1,4)) {
        Run-Checked $Executable @('--scene','forest-v03','--resource-root',$Assets,
            '--width','160','--height','90','--spp','8','--depth','10','--seed','19',
            '--threads',"$Threads",'--output',(Join-Path $Output "forest-$Threads.ppm"))
    }
    $A = (Get-FileHash (Join-Path $Output 'forest-1.ppm') -Algorithm SHA256).Hash
    $B = (Get-FileHash (Join-Path $Output 'forest-4.ppm') -Algorithm SHA256).Hash
    if ($A -ne $B) { throw 'Thread count changed output' }
    & $Executable --scene forest-v03 --resource-root 'missing-assets' --output (Join-Path $Output 'must-not-exist.ppm')
    if ($LASTEXITCODE -eq 0) { throw 'Missing resource root was not rejected' }
    Write-Host "PASS: Windows smoke checks; evidence directory: $Output"
} finally {
    Pop-Location
}
