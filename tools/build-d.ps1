# Runs build.ps1 with compiler/test temp files on D: instead of C:\...\Temp.
# Build outputs already land in <repo>\build; all build.ps1 parameters pass through.
$ErrorActionPreference = 'Stop'
$temp = 'D:\primordial\tmp'
New-Item -ItemType Directory -Force $temp | Out-Null
$initialTemp, $initialTmp = $env:TEMP, $env:TMP
$env:TEMP = $env:TMP = $temp
try {
    & (Join-Path $PSScriptRoot 'build.ps1') @args
} finally {
    $env:TEMP, $env:TMP = $initialTemp, $initialTmp
}
