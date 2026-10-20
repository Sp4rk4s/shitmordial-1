param(
    [string]$VisualStudioRoot = '',
    [string]$OutputDirectory = 'build/injector',
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
$buildRoot = Join-Path $workspace $OutputDirectory
if (!$VisualStudioRoot) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $VisualStudioRoot = & $vswhere -latest -prerelease -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if (!$VisualStudioRoot) { throw 'No Visual Studio C++ installation found.' }
$msvc = Get-ChildItem (Join-Path $VisualStudioRoot 'VC\Tools\MSVC') -Directory |
    Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
$sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$sdkVersion = Get-ChildItem (Join-Path $sdkRoot 'Include') -Directory |
    Where-Object { Test-Path (Join-Path $_.FullName 'um\Windows.h') } |
    Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
if (!$msvc -or !$sdkVersion) { throw 'MSVC or Windows SDK is missing.' }
$initialPath = $env:Path
$initialInclude = $env:INCLUDE
$initialLib = $env:LIB
try {
    foreach ($component in @(
        @{ Source = 'tools/injector'; Architecture = 'x86'; Name = 'x86' },
        @{ Source = 'tools/injector'; Architecture = 'x64'; Name = 'x64' }
    )) {
        $arch = $component.Architecture
        $env:Path = (Join-Path $msvc.FullName "bin\Hostx64\$arch") + ';' +
            (Join-Path $sdkRoot "bin\$($sdkVersion.Name)\x64") + ';' + $initialPath
        $env:INCLUDE = (Join-Path $msvc.FullName 'include') + ';' +
            (Join-Path $sdkVersion.FullName 'ucrt') + ';' +
            (Join-Path $sdkVersion.FullName 'shared') + ';' +
            (Join-Path $sdkVersion.FullName 'um') + ';' +
            (Join-Path $sdkVersion.FullName 'winrt')
        $env:LIB = (Join-Path $msvc.FullName "lib\$arch") + ';' +
            (Join-Path $sdkRoot "Lib\$($sdkVersion.Name)\ucrt\$arch") + ';' +
            (Join-Path $sdkRoot "Lib\$($sdkVersion.Name)\um\$arch")
        $destination = Join-Path $buildRoot $component.Name
        $extraArguments = @()
        & cmake -S (Join-Path $workspace $component.Source) -B $destination -G 'NMake Makefiles' -DCMAKE_BUILD_TYPE=Release @extraArguments
        if ($LASTEXITCODE) { throw "CMake configuration failed: $($component.Source)" }
        & cmake --build $destination
        if ($LASTEXITCODE) { throw "Build failed: $($component.Source)" }
    }
    if (!$SkipTests) {
        & ctest --test-dir (Join-Path $buildRoot 'x86') --output-on-failure
        if ($LASTEXITCODE) { throw 'x86 injector tests failed.' }
        & ctest --test-dir (Join-Path $buildRoot 'x64') --output-on-failure
        if ($LASTEXITCODE) { throw 'x64 injector tests failed.' }
    }
} finally {
    $env:Path = $initialPath
    $env:INCLUDE = $initialInclude
    $env:LIB = $initialLib
}
