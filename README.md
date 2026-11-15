# SEQTRAK Editor

**Transistor Labs · v0.4.2 · USB hardware editor and sequencer**

Native JUCE/C++17 source with a CMake project. The 1400 × 420 editor offers 75%, 100%, 125%, and 150% scaling. Click a track label to select it. The track LEDs indicate selection (white), mute (amber), and solo (green). Click a top-row step to toggle it. Open the corresponding lower pad to edit velocity, MIDI note, probability, and micro timing. Velocity is visible while that step is being edited.

## Connecting to SEQTRAK

1. Connect SEQTRAK by USB. Open **Connection** and choose its MIDI input and output. **Refresh Ports** updates the list after reconnecting.
2. Set the Yamaha device number if it differs from the default (displayed as 1). Click **Identify**. The editor checks Yamaha's SEQTRAK identity reply before enabling writes.
3. Select a track and click **Read Hardware** in Connection or Deep Edit. The editor requests the sound name, preset number where applicable, and voice data. It retries unread parameter addresses and reports remaining gaps. **Read All Tracks** walks through 11 tracks and then reads their Track FX, with an incomplete-read summary.
4. Use **Deep Edit** for drum/sampler controls, eight AWM2 elements, or DX common and four operators. Parameters become writable after readback. **Random** uses the Sound Lab ranges and musical DX rules, with group/parameter locks, undo, and A/B. **Hold to Audition** plays a USB note until released. **Panic** clears the queue and sends sustain off and All Sound Off.
5. **Presets** loads Yamaha factory sounds using the original bank/program sequence, then reads the track. The Sampler browser selects one of seven pads. **Track FX** reads the active effect and its parameters, applies a chosen effect, and enables parameter editing after readback.

The editor includes 855 drum sounds, 1,077 AWM2 sounds, 100 DX sounds, 392 samples and 86 FX definitions from the supplied HTML. In demo mode the catalogue and controls are browsable. Voice and FX writes use the USB drawers. The front-panel quick knobs retain their v0.2.0 local behavior. The standalone target also supports the direct USB workflow.

AU and VST3 register as instruments with host MIDI input/output and silent stereo audio. Deep Edit → Snaps still saves individual Sound Lab voice snapshots in plugin state and imports/exports the HTML editor's snapshot JSON.

## Sequencer

- In a DAW, start playback from the host. The sequencer follows tempo, PPQ, and time signature. Starting between bars arms the sequence for the next bar. During playback a seek or loop jump releases active notes and resumes at the new song position. In standalone, use **PLAY** and **BPM+ / BPM−**.
- Click a track label to select it and use **Length** to set its independent cycle from 1 to 128 sixteenth-note steps. **Page** browses all eight pages and turns Follow off. **Follow** resumes tracking the selected track's playhead page.
- Click a top-row pad to toggle a step. Click its lower pad, or hold or right-click its top pad, to set note, velocity, probability, micro timing (−50% to +50% of a sixteenth) and gate (1% to 100% of a sixteenth). **Delete** clears all eight pages of the selected track. **Mute** suppresses its notes and **Solo** limits playback to soloed tracks.
- **Swing** on the right strip sets 50% to 75%. Swing shifts alternating sixteenths on the common grid, including through odd-length track cycles. Step probability is repeatable for the same slot, track, and transport position.
- Identify SEQTRAK in **Connection** to send notes to its eleven track channels over the same USB port as SysEx. Enable **Mirror Notes to Host MIDI** to capture the sample-offset notes in a DAW that exposes plugin MIDI output. Mirroring works in demo mode. USB timing uses scheduled timestamps and depends on the operating system and USB device.

## Patterns and Program Change

1. **Patterns** has sixteen live slots. Click a numbered slot to request it, or pick a slot from the drop-down as the target for **Copy**, **Paste**, **Clear**, **Rename**, or **Switch**. Edits are saved directly into the active slot. The dot matrix displays the active slot as P01–P16.
2. Select **Immediate**, **Next beat**, or **Next bar**. Incoming Program Change 0–15 addresses slots 1–16, on any input channel. When stopped, a requested switch applies on the next processing block. When playing, a switch starts the new slot's sequence phase at the switch boundary. Hosts that route VST3 Program Change through the instrument Program parameter can also use that parameter. MIDI output routing and Program Change support depend on the host and plugin format.
3. Choose **Sound Recall**. **None** switches the sequence alone. **Presets** sends the stored factory Bank Select and Program Change messages. **Full** also sends stored voice and Track FX edits. The slot captures the factory preset selections and the voice/FX data that were read or edited for it. Use **Read All Tracks** while the slot is active to populate available hardware data. Sampler pad preset assignments are captured when loaded in the plugin. Full recalls run over the paced USB SysEx queue, so extensive sound changes can finish after the musical switch.
4. Enable **Send UI PC to Host** to emit a channel-1 Program Change on an actual slot switch requested from the panel. Incoming changes do not echo to the host. USB preset Program Change echoes are filtered.
5. **Import/Export Slot** targets the selected slot. **Import/Export Bank** handles all sixteen slots. The host saves the bank, active slot and playback settings in plugin state. Importing a bank replaces its pattern data and active slot, while host-state recall also restores the playback options.

## macOS

Install Xcode with its command-line tools, CMake 3.22 or newer, and Git. The first configure fetches JUCE 8.0.8. For an existing local JUCE checkout, append `-DSEQTRAK_JUCE_DIR=/absolute/path/to/JUCE` to the configure command.

```bash
cmake -S . -B build-mac -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-mac --config Release --target SEQTRAKEditor_AU SEQTRAKEditor_VST3 SEQTRAKEditor_Standalone
```

Open the app under `build-mac/SEQTRAKEditor_artefacts/Release/Standalone`. Copy the `.component` from `Release/AU` to `~/Library/Audio/Plug-Ins/Components/` and the `.vst3` from `Release/VST3` to `~/Library/Audio/Plug-Ins/VST3/`, then rescan in the host. Paths are relative to the artefacts directory.

CMake sets `CMAKE_OSX_DEPLOYMENT_TARGET=12.0` before initializing the project. To verify a built artifact, inspect its Mach-O build version and check for `minos 12.0`:

```bash
xcrun vtool -show-build "build-mac/SEQTRAKEditor_artefacts/Release/AU/SEQTRAK Editor.component/Contents/MacOS/SEQTRAK Editor"
auval -v aumu SqEd TrLb
```

The CMake manufacturer name is **Transistor Labs**. The AU manufacturer code is `TrLb` and the plugin code is `SqEd`.

## Windows

**Disable the SEQTRAK MIDI input and output ports in the DAW’s MIDI settings.** The plugin opens those USB ports directly and needs access to them.

Install Visual Studio 2022 with **Desktop development with C++**, the Windows SDK, CMake 3.22 or newer, and Git. In PowerShell:

```powershell
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64
cmake --build build-win --config Release --target SEQTRAKEditor_VST3 SEQTRAKEditor_Standalone
```

Launch the standalone executable from `build-win/SEQTRAKEditor_artefacts/Release/Standalone` or copy the complete `SEQTRAK Editor.vst3` bundle from the sibling `VST3` directory into `C:\Program Files\Common Files\VST3` and rescan in the DAW. AU is a macOS format.

## Hardware sequencer mode (v0.4.0)

- In **Patterns**, set **Sequencer Mode** to **PLUGIN** for the existing note sequencer or **HARDWARE** to select patterns in SEQTRAK's own sequencer. **Track Override** applies to the selected track and can follow the global mode or explicitly use either mode. Hardware tracks suppress generated plug-in notes. Start SEQTRAK's internal sequencer for those tracks to play.
- Choose a slot in the target drop-down and a track on the main panel, then set **SEQTRAK Pattern** (1–6) and **HW Mute**. The sixteen slots store these settings independently for all eleven tracks. The same Immediate, Next beat and Next bar options apply to UI and Program Change launches. The dot matrix shows the selected hardware track's pattern number in Hardware mode.
- The host exposes **Pattern &lt;track&gt;** and **Mute &lt;track&gt;** for all eleven tracks. These parameters affect hardware-routed tracks. Automation is sampled in the processing callback and changes are sent at the configured switch boundary, so Next beat can mute a track during its pattern. Launching another slot recalls that slot's stored values. Later automation can override them without changing the stored slot.
- In **Connection**, **Send tempo to SEQTRAK** sends the host BPM or standalone BPM rounded to a whole number and limited to 5–300. It sends the initial value after identification and subsequent changes. **Record Quantize** can leave the device unchanged or set Off, 1/32, 1/16T, 1/16, 1/8T, or 1/8.
- **Hardware Follow** follows track selection on SEQTRAK and flashes sampler pad hits on the dot-matrix display. It is independent of the existing sequencer page Follow control.
- The pattern, mute, tempo, record quantize, track-select and sampler-hit SysEx mappings come from community testing of SEQTRAK OS 2.x. The Yamaha device-number nibble is the displayed device number minus one.

Existing v0.3.0 banks open in PLUGIN mode by default with hardware pattern 1 and unmuted hardware tracks. Existing plug-in IDs are retained. The macOS deployment target is 12.0.
