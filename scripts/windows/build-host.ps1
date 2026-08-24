param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$BuildDirectory = "build/windows-host"
)

$ErrorActionPreference = "Stop"
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$BuildPath = Join-Path $RepositoryRoot $BuildDirectory

cmake -S $RepositoryRoot -B $BuildPath -A x64
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }

cmake --build $BuildPath --config $Configuration
if ($LASTEXITCODE -ne 0) { throw "Host build failed." }

ctest --test-dir $BuildPath -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Host tests failed." }
