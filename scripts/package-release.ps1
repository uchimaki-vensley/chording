[CmdletBinding()]
param(
    [string]$BuildDirectory = 'build',
    [string]$OutputDirectory = 'dist',
    [string]$Version
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$pinnedJuceCommit = '7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2'
$utf8NoBom = New-Object Text.UTF8Encoding($false)

function Invoke-Git {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    $result = & git -C $WorkingDirectory @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed in $WorkingDirectory."
    }
    return $result
}

function Copy-ReleaseMetadata {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'LICENSE') -Destination $Destination
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'THIRD_PARTY_NOTICES.md') -Destination $Destination

    $licensesDirectory = Join-Path $Destination 'licenses'
    New-Item -ItemType Directory -Path $licensesDirectory | Out-Null
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'ThirdParty\juce\LICENSE.md') -Destination (Join-Path $licensesDirectory 'JUCE.md')
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'ThirdParty\vst3\LICENSE.txt') -Destination (Join-Path $licensesDirectory 'VST3-SDK.txt')
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'ThirdParty\vst-ma\LICENSE.txt') -Destination (Join-Path $licensesDirectory 'Steinberg-VST-MA.txt')
}

function Remove-ExistingFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Force
    }
}

$status = @(Invoke-Git -WorkingDirectory $repositoryRoot -Arguments @('status', '--porcelain'))
if ($status.Count -ne 0) {
    throw 'Release packages must be created from a clean checkout. Commit or discard the working tree changes first.'
}

if ([string]::IsNullOrWhiteSpace($Version)) {
    $cmakeContents = [IO.File]::ReadAllText((Join-Path $repositoryRoot 'CMakeLists.txt'))
    $versionMatch = [regex]::Match($cmakeContents, 'project\s*\(\s*Chording\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)')
    if (-not $versionMatch.Success) {
        throw 'Could not read the project version from CMakeLists.txt.'
    }
    $Version = $versionMatch.Groups[1].Value
}

if ($Version -notmatch '^\d+\.\d+\.\d+([-.][0-9A-Za-z.-]+)?$') {
    throw "Invalid release version: $Version"
}

$buildRoot = if ([IO.Path]::IsPathRooted($BuildDirectory)) {
    [IO.Path]::GetFullPath($BuildDirectory)
} else {
    [IO.Path]::GetFullPath((Join-Path $repositoryRoot $BuildDirectory))
}

if (-not (Test-Path -LiteralPath $buildRoot -PathType Container)) {
    throw "Build directory does not exist: $buildRoot"
}

$midiInsertBinary = Join-Path $buildRoot 'Release\ChordingMidiInsert.dll'
$vst3Bundle = Join-Path $buildRoot 'Chording_artefacts\Release\VST3\Chording.vst3'
$cmakeCache = Join-Path $buildRoot 'CMakeCache.txt'

foreach ($requiredPath in @($midiInsertBinary, $vst3Bundle, $cmakeCache)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required release input does not exist: $requiredPath"
    }
}

$juceCacheLine = Get-Content -LiteralPath $cmakeCache | Where-Object {
    $_ -match '^(JUCE_SOURCE_DIR:STATIC|FETCHCONTENT_SOURCE_DIR_JUCE:PATH)='
} | Select-Object -First 1

if (-not $juceCacheLine) {
    throw "Could not find the JUCE source directory in $cmakeCache"
}

$juceSourceDirectory = $juceCacheLine.Substring($juceCacheLine.IndexOf('=') + 1)
$juceSourceDirectory = [IO.Path]::GetFullPath($juceSourceDirectory)
if (-not (Test-Path -LiteralPath $juceSourceDirectory -PathType Container)) {
    throw "JUCE source directory does not exist: $juceSourceDirectory"
}

$juceCommit = (Invoke-Git -WorkingDirectory $juceSourceDirectory -Arguments @('rev-parse', 'HEAD')).Trim()
if ($juceCommit -ne $pinnedJuceCommit) {
    throw "JUCE checkout is $juceCommit; expected $pinnedJuceCommit. Reconfigure the build before packaging."
}

$chordingCommit = (Invoke-Git -WorkingDirectory $repositoryRoot -Arguments @('rev-parse', 'HEAD')).Trim()
$outputRoot = if ([IO.Path]::IsPathRooted($OutputDirectory)) {
    [IO.Path]::GetFullPath($OutputDirectory)
} else {
    [IO.Path]::GetFullPath((Join-Path $repositoryRoot $OutputDirectory))
}
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null

$midiArchive = Join-Path $outputRoot "Chording-MIDI-Insert-v$Version-win64.zip"
$vst3Archive = Join-Path $outputRoot "Chording-VST3-v$Version-win64.zip"
$sourceArchive = Join-Path $outputRoot "Chording-v$Version-corresponding-source.zip"
$checksumFile = Join-Path $outputRoot 'SHA256SUMS.txt'
foreach ($path in @($midiArchive, $vst3Archive, $sourceArchive, $checksumFile)) {
    Remove-ExistingFile -Path $path
}

$temporaryBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$temporaryRoot = [IO.Path]::GetFullPath((Join-Path $temporaryBase ("chording-release-" + [guid]::NewGuid().ToString('N'))))
if (-not $temporaryRoot.StartsWith($temporaryBase, [StringComparison]::OrdinalIgnoreCase) -or
    -not ([IO.Path]::GetFileName($temporaryRoot)).StartsWith('chording-release-', [StringComparison]::Ordinal)) {
    throw "Refusing to use unexpected temporary directory: $temporaryRoot"
}

try {
    New-Item -ItemType Directory -Path $temporaryRoot | Out-Null

    $midiPackage = Join-Path $temporaryRoot "Chording-MIDI-Insert-v$Version-win64"
    New-Item -ItemType Directory -Path $midiPackage | Out-Null
    Copy-Item -LiteralPath $midiInsertBinary -Destination (Join-Path $midiPackage 'ChordingMidi.dll')
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'docs\midi-insert.md') -Destination (Join-Path $midiPackage 'INSTALL.md')
    Copy-ReleaseMetadata -Destination $midiPackage

    $vst3Package = Join-Path $temporaryRoot "Chording-VST3-v$Version-win64"
    New-Item -ItemType Directory -Path $vst3Package | Out-Null
    Copy-Item -LiteralPath $vst3Bundle -Destination $vst3Package -Recurse
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'README.md') -Destination $vst3Package
    Copy-ReleaseMetadata -Destination $vst3Package

    $sourcePackage = Join-Path $temporaryRoot "Chording-v$Version-corresponding-source"
    New-Item -ItemType Directory -Path $sourcePackage | Out-Null
    $chordingSourceArchive = Join-Path $sourcePackage "Chording-$Version-source.zip"
    $juceSourceArchive = Join-Path $sourcePackage 'JUCE-8.0.13-source.zip'
    Invoke-Git -WorkingDirectory $repositoryRoot -Arguments @('archive', '--format=zip', "--output=$chordingSourceArchive", 'HEAD') | Out-Null
    Invoke-Git -WorkingDirectory $juceSourceDirectory -Arguments @('archive', '--format=zip', "--output=$juceSourceArchive", $pinnedJuceCommit) | Out-Null

    $sourceInformation = @"
# Chording v$Version corresponding source

- Chording commit: $chordingCommit
- JUCE version: 8.0.13
- JUCE commit: $pinnedJuceCommit

Chording-$Version-source.zip contains the complete tracked Chording source and
build scripts. JUCE-8.0.13-source.zip contains the exact JUCE source selected
by the Chording build. Extract both archives before building.
"@
    [IO.File]::WriteAllText((Join-Path $sourcePackage 'SOURCE_INFO.md'), $sourceInformation, $utf8NoBom)
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'LICENSE') -Destination $sourcePackage
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'THIRD_PARTY_NOTICES.md') -Destination $sourcePackage

    Compress-Archive -LiteralPath $midiPackage -DestinationPath $midiArchive -CompressionLevel Optimal
    Compress-Archive -LiteralPath $vst3Package -DestinationPath $vst3Archive -CompressionLevel Optimal
    Compress-Archive -LiteralPath $sourcePackage -DestinationPath $sourceArchive -CompressionLevel Optimal

    $checksumLines = foreach ($archive in @($midiArchive, $vst3Archive, $sourceArchive)) {
        $hash = Get-FileHash -LiteralPath $archive -Algorithm SHA256
        "$($hash.Hash.ToLowerInvariant())  $([IO.Path]::GetFileName($archive))"
    }
    [IO.File]::WriteAllLines($checksumFile, $checksumLines, $utf8NoBom)

    Get-Item -LiteralPath $midiArchive, $vst3Archive, $sourceArchive, $checksumFile |
        Select-Object Name, Length, FullName
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        $resolvedTemporaryRoot = [IO.Path]::GetFullPath($temporaryRoot)
        if (-not $resolvedTemporaryRoot.StartsWith($temporaryBase, [StringComparison]::OrdinalIgnoreCase) -or
            -not ([IO.Path]::GetFileName($resolvedTemporaryRoot)).StartsWith('chording-release-', [StringComparison]::Ordinal)) {
            throw "Refusing to remove unexpected temporary directory: $resolvedTemporaryRoot"
        }
        Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
    }
}
