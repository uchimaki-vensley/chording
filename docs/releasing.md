# Release procedure

Chording binary releases are distributed under `AGPL-3.0-only`. Each release
must provide the exact corresponding source used to build its binaries,
including JUCE.

## Prepare

1. Use a clean checkout at the release commit.
2. Build and test the Release configuration as documented in `README.md`.
3. Run the packaging script from the repository root:

   ```powershell
   powershell -ExecutionPolicy Bypass -File scripts/package-release.ps1 `
     -BuildDirectory build -OutputDirectory dist
   ```

The script verifies that the checkout is clean, checks the JUCE commit, and
creates:

- `Chording-MIDI-Insert-vX.Y.Z-win64.zip`
- `Chording-VST3-vX.Y.Z-win64.zip`
- `Chording-vX.Y.Z-corresponding-source.zip`
- `SHA256SUMS.txt`

The binary archives contain the Chording license, third-party notices, and all
third-party license texts relevant to the packaged formats. The corresponding
source archive contains separate Git archives for the tracked Chording source
at the release commit and the full JUCE source at the pinned commit.

## Publish

Create a draft GitHub Release from the matching version tag and attach all four
files. Review the archive contents and checksum file before publishing.

```powershell
gh release create vX.Y.Z dist\*.zip dist\SHA256SUMS.txt `
  --draft --generate-notes --title "Chording vX.Y.Z"
```

Do not publish a binary without its corresponding source archive and license
files. Do not replace assets on an existing release; publish a new version.
