param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [string]$BuildDirectory = (Join-Path $env:USERPROFILE '.codex/builds/SootheClone'),
    [string]$JuceDirectory = 'C:/JUCE'
)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio C++ tools were not found.' }
$cmake = Join-Path $visualStudio 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
# ASCII build paths avoid an encoding failure in JUCE's Windows helper commands.
if ($BuildDirectory -match '[^\x00-\x7F]') { throw 'Choose an ASCII-only BuildDirectory for JUCE helper compatibility.' }
$previousPlatform = $env:CMAKE_GENERATOR_PLATFORM
try {
    Remove-Item Env:CMAKE_GENERATOR_PLATFORM -ErrorAction SilentlyContinue
    & $cmake -S $PSScriptRoot -B $BuildDirectory -G 'Visual Studio 18 2026' -A x64 "-DJUCE_PATH=$JuceDirectory"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & $cmake --build $BuildDirectory --config $Configuration --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    & $ctest --test-dir $BuildDirectory -C $Configuration --output-on-failure -j 2
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
} finally {
    $env:CMAKE_GENERATOR_PLATFORM = $previousPlatform
}
