param(
    [ValidateSet("Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$BuildDirectory = "build/windows-host",
    [string]$OutputDirectory = "build/packages",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$BuildPath = Join-Path $RepositoryRoot $BuildDirectory

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build-host.ps1") `
        -Configuration $Configuration -BuildDirectory $BuildDirectory
    if ($LASTEXITCODE -ne 0) { throw "Build and test step failed." }
}

$StagePath = Join-Path $BuildPath "package/$Configuration/picosd-host"
$PackageRoot = Join-Path $RepositoryRoot $OutputDirectory
$ArchivePath = Join-Path $PackageRoot "picosd-host-windows-x64.zip"

Remove-Item $StagePath -Recurse -Force -ErrorAction SilentlyContinue
New-Item $StagePath -ItemType Directory -Force | Out-Null
New-Item $PackageRoot -ItemType Directory -Force | Out-Null

cmake --install $BuildPath --config $Configuration --prefix $StagePath
if ($LASTEXITCODE -ne 0) { throw "Host installation failed." }

Copy-Item (Join-Path $RepositoryRoot "LICENSE") $StagePath
Copy-Item (Join-Path $RepositoryRoot "README.md") $StagePath
New-Item (Join-Path $StagePath "docs") -ItemType Directory | Out-Null
Copy-Item (Join-Path $RepositoryRoot "docs/windows_host_prerequisites.md") `
    (Join-Path $StagePath "docs")

Remove-Item $ArchivePath -Force -ErrorAction SilentlyContinue
Compress-Archive -Path $StagePath -DestinationPath $ArchivePath
Write-Host "Created $ArchivePath"
