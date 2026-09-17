# Cubase Artist 14 MIDI Insert

Chording MIDI is a Windows 64-bit MIDI Insert for Cubase Artist 14. It reads the
MIDI events before the instrument, updates the existing Chording display, and
passes every event to the instrument unchanged. Sustain pedal CC64 therefore
continues to control the instrument but does not add released notes to chord
recognition.

The implementation uses Steinberg's legacy VST Module Architecture MIDI-FX API.
Its retained interface headers and license are in `ThirdParty/vst-ma`.

## Build

Run in Developer PowerShell for Visual Studio 2022:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The MIDI Insert is generated at:

```text
build/Release/ChordingMidiInsert.dll
```

## Install and use

1. Close Cubase.
2. For a source build, copy `ChordingMidiInsert.dll` as `ChordingMidi.dll` and
   include `ThirdParty/vst-ma/LICENSE.txt` in a dedicated directory under
   `C:\Program Files\Steinberg\Cubase 14\Components`. The release package
   already uses the `ChordingMidi.dll` name and includes all license files.
3. Start Cubase and select the instrument track.
4. Open **MIDI Inserts** in the Inspector and select **Chording MIDI**.
5. Click its edit button to open the Chording display.

Do not add the regular `Chording.vst3` to the instrument's audio Inserts for
this workflow. Cubase sends audio, but not the instrument track's incoming MIDI,
to that position.

The MIDI Insert clears recognition on transport start, stop, cycle jump, and
position changes to prevent stuck notes. Live note-on/off messages and recorded
notes represented by start plus duration are both tracked. Project settings and
progression history use the component's persistent chunk.
