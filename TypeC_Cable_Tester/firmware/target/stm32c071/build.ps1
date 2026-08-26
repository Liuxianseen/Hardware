[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$OfficeSilent
)

$ErrorActionPreference = 'Stop'
$projectDir = $PSScriptRoot
$firmwareDir = (Resolve-Path (Join-Path $projectDir '..\..')).Path
$workspaceDir = (Resolve-Path (Join-Path $projectDir '..\..\..\..')).Path
$coreDir = Join-Path $workspaceDir '.pio-core-typec'
$releaseDir = Join-Path $projectDir 'release'
$environmentName = if ($OfficeSilent) { 'office_silent' } else { 'release' }
$versionName = if ($OfficeSilent) { '0.3.6-OFFICE-SILENT' } else { '0.3.6' }
$artifactSuffix = if ($OfficeSilent) { 'v0.3.6-office-silent' } else { 'v0.3.6' }
$buildDir = Join-Path $workspaceDir ".pio-build-typec-c071\$environmentName"

$pioCommand = Get-Command platformio -ErrorAction SilentlyContinue
$pioFallback = Join-Path ([Environment]::GetFolderPath('UserProfile')) '.platformio\penv\Scripts\platformio.exe'
$pioPath = if ($pioCommand) {
    $pioCommand.Source
} else {
    $pioFallback
}

if (-not (Test-Path -LiteralPath $pioPath -PathType Leaf)) {
    throw 'PlatformIO Core was not found. Install PlatformIO Core, then rerun this script.'
}

New-Item -ItemType Directory -Force -Path $coreDir, $releaseDir | Out-Null
$env:PLATFORMIO_CORE_DIR = $coreDir

if ($Clean) {
    & $pioPath run --project-dir $projectDir --environment $environmentName --target clean
    if ($LASTEXITCODE -ne 0) {
        throw "PlatformIO clean failed with exit code $LASTEXITCODE."
    }
}

& $pioPath run --project-dir $projectDir --environment $environmentName
if ($LASTEXITCODE -ne 0) {
    throw "PlatformIO build failed with exit code $LASTEXITCODE."
}

$elfSource = Join-Path $buildDir 'firmware.elf'
$binSource = Join-Path $buildDir 'firmware.bin'
$mapSource = Join-Path $buildDir 'firmware.map'
$objcopy = Join-Path $coreDir 'packages\toolchain-gccarmnoneeabi\bin\arm-none-eabi-objcopy.exe'
$gcc = Join-Path $coreDir 'packages\toolchain-gccarmnoneeabi\bin\arm-none-eabi-gcc.exe'
$sizeTool = Join-Path $coreDir 'packages\toolchain-gccarmnoneeabi\bin\arm-none-eabi-size.exe'

foreach ($requiredFile in @($elfSource, $binSource, $objcopy, $gcc, $sizeTool)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Expected build file is missing: $requiredFile"
    }
}

$baseName = "TypeC_Tester_STM32C071_$artifactSuffix"
$elfTarget = Join-Path $releaseDir "$baseName.elf"
$binTarget = Join-Path $releaseDir "$baseName.bin"
$hexTarget = Join-Path $releaseDir "$baseName.hex"
$mapTarget = Join-Path $releaseDir "$baseName.map"

Copy-Item -LiteralPath $elfSource -Destination $elfTarget -Force
Copy-Item -LiteralPath $binSource -Destination $binTarget -Force
if (Test-Path -LiteralPath $mapSource -PathType Leaf) {
    Copy-Item -LiteralPath $mapSource -Destination $mapTarget -Force
}

& $objcopy -O ihex $elfSource $hexTarget
if ($LASTEXITCODE -ne 0) {
    throw "HEX export failed with exit code $LASTEXITCODE."
}

$artifactPaths = @($elfTarget, $binTarget, $hexTarget)
$hashLines = foreach ($artifact in $artifactPaths) {
    $hash = (Get-FileHash -LiteralPath $artifact -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $(Split-Path -Leaf $artifact)"
}
$hashFileName = if ($OfficeSilent) { 'SHA256SUMS_OFFICE_SILENT.txt' } else { 'SHA256SUMS.txt' }
$hashLines | Set-Content -LiteralPath (Join-Path $releaseDir $hashFileName) -Encoding ascii

$gccVersion = (& $gcc --version | Select-Object -First 1)
$pioVersion = (& $pioPath --version)
$sizeLine = (& $sizeTool $elfSource | Select-Object -Last 1).Trim()
$sizeColumns = $sizeLine -split '\s+'
$textBytes = [int]$sizeColumns[0]
$dataBytes = [int]$sizeColumns[1]
$bssBytes = [int]$sizeColumns[2]
$flashBytes = $textBytes + $dataBytes
$ramReservedBytes = $dataBytes + $bssBytes
$flashPercent = [Math]::Round(($flashBytes * 100.0) / 65536.0, 1)
$ramPercent = [Math]::Round(($ramReservedBytes * 100.0) / 24576.0, 1)
$buildInfoName = if ($OfficeSilent) { 'BUILD_INFO_OFFICE_SILENT.txt' } else { 'BUILD_INFO.txt' }
@(
    "Firmware: Type-C Cable Tester $versionName"
    'Target: STM32C071G8U6 (64 KiB Flash / 24 KiB RAM)'
    "Built: $([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))"
    "PlatformIO: $pioVersion"
    "Compiler: $gccVersion"
    'Framework: Arduino_Core_STM32 2.12.0 / framework package 4.21200.0'
    $(if ($OfficeSilent) { 'Buzzer policy: PA5 forced low (office silent)' } else { 'Buzzer policy: enabled (factory)' })
    "Flash image: $flashBytes bytes ($flashPercent%)"
    "RAM reserved: $ramReservedBytes bytes ($ramPercent%, includes linker heap/stack reservation)"
    "GNU size (text data bss dec hex): $($sizeColumns[0..4] -join ' ')"
    'Flash base for raw BIN: 0x08000000'
) | Set-Content -LiteralPath (Join-Path $releaseDir $buildInfoName) -Encoding utf8

Write-Host "Release artifacts written to: $releaseDir"
Get-ChildItem -LiteralPath $releaseDir | Select-Object Name, Length, LastWriteTime
