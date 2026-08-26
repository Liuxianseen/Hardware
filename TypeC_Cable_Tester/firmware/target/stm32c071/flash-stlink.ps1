[CmdletBinding()]
param(
    [string]$Firmware,
    [switch]$OfficeSilent,
    [string]$StLinkSerial = ''
)

$ErrorActionPreference = 'Stop'
if ($OfficeSilent -and $PSBoundParameters.ContainsKey('Firmware')) {
    throw 'Use either -OfficeSilent or -Firmware, not both.'
}
if ([string]::IsNullOrWhiteSpace($Firmware)) {
    $firmwareName = if ($OfficeSilent) {
        'TypeC_Tester_STM32C071_v0.3.6-office-silent.elf'
    } else {
        'TypeC_Tester_STM32C071_v0.3.6.elf'
    }
    $Firmware = Join-Path $PSScriptRoot "release\$firmwareName"
}

$programmerCandidates = @(
    'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe',
    'C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'
)
$programmer = $programmerCandidates | Where-Object {
    Test-Path -LiteralPath $_ -PathType Leaf
} | Select-Object -First 1

if (-not $programmer) {
    throw 'STM32CubeProgrammer CLI was not found. Install STM32CubeProgrammer first.'
}
if (-not (Test-Path -LiteralPath $Firmware -PathType Leaf)) {
    throw "Firmware file was not found: $Firmware"
}

$resolvedFirmware = (Resolve-Path -LiteralPath $Firmware).Path
Write-Host "Programming and verifying: $resolvedFirmware"
$connectionArguments = @('port=SWD', 'mode=UR', 'reset=HWrst', 'freq=1000')
if (-not [string]::IsNullOrWhiteSpace($StLinkSerial)) {
    $connectionArguments += "sn=$StLinkSerial"
}
& $programmer -c @connectionArguments -w $resolvedFirmware -v -rst
if ($LASTEXITCODE -ne 0) {
    throw "STM32CubeProgrammer failed with exit code $LASTEXITCODE."
}

Write-Host 'Programming, verification, and target reset completed.'
