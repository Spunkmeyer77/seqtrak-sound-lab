import sys
from reportlab.lib.pagesizes import A4
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Image, KeepTogether, ListFlowable, ListItem,
                                Table, TableStyle, PageBreak)

R = sys.argv[1]
V = "0.5.2"
FIG = "/home/claude/figs"
ORANGE = colors.HexColor("#E8743B")
GREY = colors.HexColor("#3A3D40")
LIGHT = colors.HexColor("#FBE3D6")
H = ParagraphStyle("h", fontName="Helvetica-Bold", fontSize=15, textColor=ORANGE, spaceBefore=10, spaceAfter=6)
H2 = ParagraphStyle("h2", fontName="Helvetica-Bold", fontSize=11.5, textColor=GREY, spaceBefore=6, spaceAfter=3)
B = ParagraphStyle("b", fontName="Helvetica", fontSize=10, leading=14, textColor=GREY, spaceAfter=5)
C = ParagraphStyle("c", fontName="Helvetica-Oblique", fontSize=8.5, leading=11, textColor=GREY, spaceAfter=6)
T = ParagraphStyle("t", fontName="Helvetica-Bold", fontSize=26, leading=32, textColor=GREY, spaceAfter=4)
S = ParagraphStyle("s", fontName="Helvetica", fontSize=12, textColor=ORANGE, spaceAfter=14)
W = 170 * mm

def footer(c, d):
    c.saveState(); c.setFont("Helvetica", 8); c.setFillColor(GREY)
    c.drawString(20*mm, 10*mm, f"SEQTRAK Editor v{V}  ·  Transistor Labs  ·  ticcoross@gmail.com")
    c.drawRightString(190*mm, 10*mm, str(d.page)); c.restoreState()

def bl(items):
    return ListFlowable([ListItem(Paragraph(i, B), leftIndent=10) for i in items], bulletType="bullet", start="•", leftIndent=12)

def nl(items):
    return ListFlowable([ListItem(Paragraph(i, B), leftIndent=12) for i in items], bulletType="1", leftIndent=16)

def img(name, w=W):
    from PIL import Image as P
    path = name if name.startswith("/") else f"{R}/{name}"
    iw, ih = P.open(path).size
    return Image(path, width=w, height=w*ih/iw)

def fig(name, w=W, caption=None):
    parts = [img(f"{FIG}/fig_{name}.png", w)]
    if caption: parts.append(Paragraph(caption, C))
    return parts

def ch(title, *flow):
    return KeepTogether([Paragraph(title, H), *flow, Spacer(1, 6)])

def table(rows, widths):
    t = Table([[Paragraph(c, B) for c in r] for r in rows], colWidths=widths)
    t.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, 0), LIGHT), ("VALIGN", (0, 0), (-1, -1), "TOP"),
                           ("LINEBELOW", (0, 0), (-1, -1), 0.3, colors.HexColor("#E0C8B8")),
                           ("TOPPADDING", (0, 0), (-1, -1), 2), ("BOTTOMPADDING", (0, 0), (-1, -1), 2)]))
    return t

story = [
    Paragraph("SEQTRAK Editor", T),
    Paragraph(f"User Manual · v{V} · Transistor Labs", S),
    *fig("main", W, "The SEQTRAK Editor panel. The numbers are explained in chapter 6, The Panel at a Glance."),
    ch("1. Introduction",
       Paragraph("<b>SEQTRAK Editor powers up your Yamaha SEQTRAK: much more pattern memory (64 patterns instead of 6) "
                 "and a pattern chain for whole songs, plus full sound editing over USB.</b>", B),
       Paragraph("It is an AU / VST3 instrument plugin and a standalone app. It edits every sound on SEQTRAK (drums, "
                 "AWM2 synths, DX FM and the sampler), plays SEQTRAK from its own 11-track step sequencer, stores 64 "
                 "patterns in 4 banks, chains them into songs, and can print any pattern into SEQTRAK's own memory.", B),
       bl(["<b>Sound editing:</b> 2,032 presets and 392 samples, Deep Edit of every parameter, Track FX, randomizer, snapshots.",
           "<b>Sequencer:</b> 11 tracks, up to 128 steps each, per-step note, velocity, probability, micro timing, gate, flam and chords.",
           "<b>Patterns:</b> 4 banks × 16 slots, Program Change 0–63, Pattern Chain, Print to SEQTRAK.",
           "<b>Live control:</b> orange knobs and faders, mute/solo and track volumes sent to SEQTRAK as you move them."])),
    ch("2. System Requirements",
       table([["Item", "Requirement"],
              ["macOS", "macOS 12 Monterey or later, Apple Silicon or Intel (universal build). AU, VST3 and standalone app."],
              ["Windows", "Windows 10 or 11, 64-bit. VST3 and standalone app."],
              ["Host (DAW)", "Any AU or VST3 host that supports instrument plugins with MIDI input, for example Ableton Live 11/12, "
                             "Logic Pro, Bitwig Studio, Reaper, Cubase or Studio One. Or use the standalone app without a DAW."],
              ["Hardware", "Yamaha SEQTRAK. SEQTRAK OS 2.00 or later is recommended (the hardware-mode, chord and print functions "
                           "follow the OS 2.x MIDI specification)."],
              ["Connection", "USB data cable from SEQTRAK to the computer (USB-C on SEQTRAK). Bluetooth MIDI and Wi-Fi are not used. "
                             "One SEQTRAK per editor window."],
              ["MIDI ports", "The plugin opens SEQTRAK's USB MIDI ports itself. On Windows, switch the SEQTRAK ports off in your "
                             "DAW's MIDI settings (Windows allows one program per port). On macOS the ports can be shared."],
              ["Screen", "1400 × 420 pixels at 100%. The window can be scaled to 75% (1050 × 315), 125% or 150%."],
              ["Disk / CPU", "About 25 MB on macOS, 6 MB on Windows. Very light on CPU: the plugin makes no audio itself, "
                             "SEQTRAK makes the sound."],
              ["Audio", "Listen to SEQTRAK's own headphone/line output, or its USB audio in your audio interface or DAW."]],
             [32*mm, 138*mm])),
    ch("3. Installation", bl([
        "<b>macOS:</b> open the .pkg installer. It installs the AU in /Library/Audio/Plug-Ins/Components, the VST3 in "
        "/Library/Audio/Plug-Ins/VST3 and SEQTRAK Editor.app in /Applications.",
        "<b>Windows:</b> run the Setup .exe. It installs the VST3 in C:\\Program Files\\Common Files\\VST3 and the "
        "standalone app with a Start-menu shortcut (Transistor Labs).",
        "Plugins-only .zip downloads are also provided for manual installation.",
        "Restart or rescan plugins in your DAW. The plugin appears under <b>Transistor Labs</b>, as an instrument. "
        "In Ableton, drop it on a MIDI track."])),
    ch("4. Quick Start: First Connection (do this first)",
       *fig("connect", 150*mm),
       Paragraph("<b>The editor does nothing on SEQTRAK until it is connected.</b> The first time you load the plugin "
                 "(and whenever you reconnect SEQTRAK), follow these steps in order:", B),
       *fig("connection", 118*mm, "The CONNECTION drawer. 1-2 MIDI ports, 3 device number, 4 IDENTIFY, 5 read sounds, "
            "6 refresh ports, 7 send sequencer notes to the DAW too, 8 status, 9 tempo to SEQTRAK, 10 record quantize."),
       nl(["Connect SEQTRAK with a USB <b>data</b> cable and switch it on.",
           "Open the plugin and press <b>CONNECTION</b> (bottom row of the orange section).",
           "Choose <b>SEQTRAK</b> in <b>MIDI INPUT</b> (1) and in <b>MIDI OUTPUT</b> (2). Not listed? Press <b>REFRESH PORTS</b> (6). Both must be set.",
           "Leave <b>DEVICE</b> (3) at 1 unless you changed SEQTRAK's device number.",
           "Press <b>IDENTIFY</b> (4). When SEQTRAK answers, the status (8) confirms it and the label at the top changes from "
           "DEMO to <b>USB MIDI</b>. Only now are edits, sounds and notes sent to SEQTRAK.",
           "Optional: press <b>READ ALL TRACKS</b> (5) so the editor shows the sounds already on SEQTRAK."]),
       Paragraph("<b>Windows:</b> before step 3, switch off the SEQTRAK MIDI input and output in your DAW's MIDI settings. "
                 "<b>No answer to IDENTIFY?</b> Check the cable (some USB cables only charge), the two ports and the device number, "
                 "then press IDENTIFY again.", B)),
    ch("5. macOS Security Warning",
       Paragraph("The installer is not notarised by Apple, so macOS may say it “cannot be opened because the developer cannot be verified”. To open it:", B),
       bl(["Right-click (or Control-click) the .pkg and choose <b>Open</b>, then click <b>Open</b> again.",
           "Or open <b>System Settings → Privacy &amp; Security</b>, scroll down and click <b>Open Anyway</b> next to the SEQTRAK Editor message.",
           "If a host still refuses the plugin, run in Terminal: <font face='Courier'>sudo xattr -dr com.apple.quarantine "
           "\"/Library/Audio/Plug-Ins/Components/SEQTRAK Editor.component\"</font> (and the same for the .vst3)."])),
    ch("More Patterns Than the Hardware",
       *fig("memory", 150*mm),
       Paragraph("<b>SEQTRAK Editor powers up your SEQTRAK hardware: it adds far more pattern memory and a pattern chain "
                 "that the unit doesn't have on its own.</b> SEQTRAK holds 6 patterns per track; the editor gives you 64: "
                 "16 patterns × 4 banks (A–D).", B),
       bl(["Every pattern stores the sequence of all 11 tracks plus their sounds, and all 64 are saved with your DAW project.",
           "Switch patterns from the editor, by Program Change 0–63, or automatically with the <b>Pattern Chain</b>: up to 16 "
           "steps, each a pattern and a length in bars, switching exactly on the bar and looping if you want, to arrange whole songs.",
           "Want a pattern on SEQTRAK itself? <b>Print to SEQTRAK</b> writes any of the 64 into one of SEQTRAK's 6 slots."])),
    ch("6. The Panel at a Glance",
       *fig("main", W),
       nl(["<b>Top bar:</b> LENGTH (steps of the selected track), FOLLOW (pads follow the playing page; orange = on), "
           "HELP (hover help; orange = on), window size, ABOUT.",
           "<b>Left strip:</b> VOL+ / VOL− (selected track volume), DELETE (clear the track's steps), MUTE, SOLO, PAGE, PLAY (standalone).",
           "<b>Track knobs:</b> ALL, the seven drum tracks, SYNTH 1, SYNTH 2, DX, SAMPLER. Click a name to select the track "
           "(thin dark frame); turn the knob for its volume. LEDs: white = selected, amber = muted, green = solo.",
           "<b>Status line and page numbers:</b> track, page, swing, FOLLOW, tempo and transport. Click a page number to show that page.",
           "<b>Step pads:</b> top row = steps on/off (drag to paint), bottom row = open the step editor.",
           "<b>Beat marks:</b> small ticks on steps 1, 5, 9 and 13 for 4/4.",
           "<b>Parameter list:</b> chooses what the four orange knobs control (see chapter 9).",
           "<b>Four orange knobs:</b> edit the selected track live on SEQTRAK. Dimmed knobs have no MIDI control (use Deep Edit).",
           "<b>FX:</b> MASTER/SINGLE switch and FX LEVEL fader.",
           "<b>Display:</b> sound name, preset number, pattern and the value of the control you are moving.",
           "<b>HIGH PASS and REPEATER:</b> master effects 2 and 3. REPEATER snaps to its 5 positions (darker stripes).",
           "<b>Drawer buttons:</b> PRESETS, DEEP EDIT, TRACK FX, RANDOM, PATTERNS, CONNECTION.",
           "<b>Right strip:</b> USB and MIDI (open Connection), BPM+ / BPM− (standalone), SWING (50–75%), PROJECT (Patterns)."])),
    ch("7. Tracks, Volume, Mute and Solo", bl([
        "<b>Select a track</b> by clicking its name under the knob. Everything in the orange section, the pads and the drawers "
        "then works on that track.",
        "<b>Volume:</b> turn the track's knob, or use VOL+ / VOL− on the left strip. The volume is sent to SEQTRAK.",
        "<b>MUTE / SOLO:</b> select a track, press MUTE or SOLO on the left strip; press again to release. The LEDs light amber "
        "(mute) or green (solo). Mute and solo affect the editor's sequencer <b>and</b> are sent to SEQTRAK, so they also work "
        "while SEQTRAK's own sequencer plays. SEQTRAK solos one track at a time: with several solos in the editor, SEQTRAK follows "
        "the last one you pressed.",
        "Mutes and solos are stored in each pattern."])),
    ch("8. Sound Design: Presets",
       *fig("presets", 115*mm, "1 search, 2 category, 3 preset list, 4 status, 5 load."),
       bl(["855 drum, 1,077 AWM2 and 100 DX sounds plus 392 samples. Type a name or number (1) or pick a category (2).",
           "Highlight a sound in the list (3) and press <b>LOAD ON TRACK</b> (5), or simply <b>double-click</b> it (or press Return). "
           "On the Sampler, choose the pad and press LOAD ON PAD.",
           "The browser remembers, for each track, the category, search and highlighted sound when you come back."])),
    ch("Sound Design: Deep Edit, Track FX, Random and Snapshots",
       *fig("deep", 115*mm, "1 element / operator, 2 read and audition, 3 snapshots, 4 parameters, 5 value faders, 6 locks."),
       bl(["<b>Deep Edit</b> shows every parameter of the track's sound: drum and sampler controls, the eight AWM2 elements, "
           "or the DX common settings and four operators (1). Parameters become editable after <b>READ HARDWARE</b> (2). "
           "<b>Double-click a fader</b> to reset it to its default; Undo brings the old value back. <b>HOLD TO AUDITION</b> plays a "
           "test note.",
           "<b>Track FX:</b> READ FX, choose one of 86 effects, APPLY EFFECT, then edit its parameters.",
           "<b>Random:</b> Subtle, Medium or Wild randomizing; <b>LOCK</b> (6) keeps parameters. Undo, Capture A, Compare A/B.",
           "<b>Snapshots</b> (3) save voices with your project and import/export the Sound Lab web editor's JSON format."])),
    ch("9. The Orange Section",
       table([["Page", "What the knobs send to SEQTRAK (selected track)"],
              ["PAN, VOLUME", "Pan (CC 10), Volume (CC 7)"],
              ["ATTACK, DECAY", "EG attack (CC 73), decay/release (CC 75)"],
              ["FILTER, RESONANCE", "Cutoff (CC 74), resonance (CC 71)"],
              ["REVERB, DELAY", "Reverb send (CC 91), delay send (CC 94)"],
              ["PITCH", "Drum pitch on drum tracks (CC 25), glide on synth tracks (CC 5)"],
              ["MONO/POLY", "Mono / Poly / Chord on synth and DX tracks (CC 26)"],
              ["CHORD", "Mode (Mono/Poly/Chord), project Key (C–B) and Scale (1–8)"],
              ["MIXER", "Volume, pan, reverb and delay together"],
              ["Other pages", "No MIDI control on SEQTRAK: the knobs are dimmed; edit these in Deep Edit"]],
             [40*mm, 130*mm]),
       Spacer(1, 4),
       bl(["<b>FX LEVEL</b> drives Master Effect 1 (MASTER) or the selected track's Single Effect (SINGLE). Choose effect types in TRACK FX.",
           "<b>HIGH PASS</b> and <b>REPEATER</b> drive Master Effects 2 and 3. REPEATER snaps to its 5 positions.",
           "SEQTRAK doesn't report these values back, so a knob shows its last position, not the sound's current value."])),
    ch("10. Sequencer",
       bl(["In a DAW, start playback from the DAW: the sequencer follows its tempo, position and time signature (starting "
           "mid-bar, it waits for the next bar). In the standalone app use PLAY and BPM+ / BPM−.",
           "<b>Steps:</b> click a top pad to turn a step on or off. <b>Paint:</b> keep the button held and drag across the pads "
           "to set many steps at once (starting on an empty step turns steps on, starting on an active step clears them).",
           "<b>Length:</b> each track has its own length (LENGTH, 1–128 steps), so tracks can run polymetric.",
           "<b>Pages:</b> the numbers next to the track name are the pages this track uses (16 steps each). Click one to show it, "
           "or press PAGE to step through them. A light bar marks the playing page.",
           "<b>FOLLOW</b> (orange) makes the pads follow the playing page. Choosing a page by hand turns FOLLOW off, so the pads "
           "stay where you are editing.",
           "<b>Swing</b> 50–75% (right strip). DELETE clears the selected track. Notes go to SEQTRAK's 11 channels; "
           "MIRROR NOTES TO HOST MIDI (Connection) also sends them to your DAW for recording."])),
    ch("The Step Editor",
       *fig("step", 115*mm),
       Paragraph("Open it by holding or right-clicking a top pad, or clicking the pad below it. It stays open while the pattern "
                 "plays, even if the chain switches patterns; its title shows which pattern you are editing.", B),
       nl(["<b>ACTIVE:</b> step on/off.",
           "<b>CHORD</b> (Synth 1, Synth 2, DX): Off, Major, Minor, 7, Maj7, m7, m9, Sus2, Sus4, Add9, Dim, Aug, Power — built on the MIDI note.",
           "<b>VELOCITY</b> 1–127.", "<b>MIDI NOTE</b> 0–127 (the chord's root).",
           "<b>PROBABILITY</b> 0–100%: chance that the step plays each time.",
           "<b>MICRO TIMING</b> −50 to +50% of a step: play early or late.",
           "<b>GATE</b> 1–100% of a step: note length.",
           "<b>FLAM</b> 0–50% of a step: adds a softer grace note before the hit."])),
    ch("Flam and Chords",
       *fig("flamchord", 150*mm),
       bl(["<b>Flam:</b> a grace note at 60% of the step's velocity, placed before the hit by the FLAM amount. 10–20% is a "
           "classic tight flam. Flam plays from the editor only (SEQTRAK can't store it).",
           "<b>Chords:</b> all chord notes start together and last for the gate. They play live and are printed to SEQTRAK."])),
    ch("11. Patterns and Banks",
       *fig("patterns", 125*mm),
       nl(["<b>CHAIN:</b> opens the Pattern Chain (chapter 12).",
           "<b>Banks A B C D:</b> show each bank's 16 slots.",
           "<b>Slots</b> A01–D16: click to switch (with the switch timing below). The playing slot is dark.",
           "<b>Slot menu, name and RENAME.</b>",
           "<b>COPY, PASTE, CLEAR, SWITCH</b> act on the slot chosen in the menu.",
           "<b>SWITCH QUANTIZE</b> (Immediate, Next beat, Next bar) and <b>SOUND RECALL</b> (None, Presets, Full: also send the slot's sounds).",
           "<b>PC OUT TO DAW:</b> clicking a slot also sends a Program Change to your DAW, to record slot changes in a clip.",
           "<b>IMPORT / EXPORT</b> a slot or the whole bank as JSON.",
           "<b>SEQUENCER MODE:</b> PLUGIN or HARDWARE (chapter 13).",
           "<b>TRACK OVERRIDE:</b> one track follows the mode or uses PLUGIN / HARDWARE.",
           "<b>SEQTRAK PATTERN and MUTE ON SEQTRAK:</b> what SEQTRAK plays on this track in HARDWARE mode (dimmed in PLUGIN mode).",
           "<b>PRINT:</b> opens Print to SEQTRAK (chapter 14)."]),
       Paragraph("<b>Program Change 0–63</b> on any channel selects a pattern: 0–15 = bank A, 16–31 = B, 32–47 = C, 48–63 = D. "
                 "Projects from older versions open in bank A.", B)),
    ch("12. Pattern Chain",
       *fig("chain", 115*mm),
       *fig("chaintime", 150*mm),
       nl(["<b>CHAIN ON</b> runs the chain when playback starts.", "<b>LOOP</b> repeats it; with LOOP off it stays on the last step.",
           "<b>CLEAR</b> empties the chain.", "The status line shows the playing step and slot.",
           "Choose a slot for each step…", "…and its length in bars (1–64).",
           "The chain ends at the first step set to “(end)”."]),
       Paragraph("Every change lands exactly on the bar, whatever Switch Quantize is set to. You can still switch slots by hand "
                 "while the chain runs. The chain is saved with your project.", B)),
    ch("13. Sequencer Modes",
       *fig("modes", 150*mm),
       bl(["<b>PLUGIN</b> (default): the editor's sequencer plays SEQTRAK. All step features work (probability, flam, chords).",
           "<b>HARDWARE:</b> SEQTRAK's own sequencer plays. Each slot stores, per track, which SEQTRAK pattern (1–6) to play and "
           "MUTE ON SEQTRAK; launching the slot sends them. Start SEQTRAK's sequencer for these tracks.",
           "<b>TRACK OVERRIDE</b> mixes both: some tracks from the editor, others from SEQTRAK.",
           "The DAW sees 22 automatable parameters, <b>Pattern &lt;track&gt;</b> and <b>Mute &lt;track&gt;</b>; with Next beat "
           "mutes can land mid-pattern. These messages follow community testing on SEQTRAK OS 2.x."])),
    ch("14. Print to SEQTRAK",
       *fig("print", 115*mm),
       Paragraph("<b>First time: test on a SEQTRAK pattern you don't mind losing</b> (or back up your project with the SEQTRAK app first).", B),
       nl(["<b>FROM SLOT:</b> the editor pattern to print (any of the 64).",
           "<b>TO SEQTRAK PATTERN:</b> SEQTRAK slot 1–6 to write into (it is overwritten on the chosen tracks).",
           "<b>TRACKS:</b> which tracks to include (ALL selects every track).",
           "<b>Stop SEQTRAK and your DAW</b>, then press <b>PRINT TO SEQTRAK</b>.",
           "The status counts the messages down; wait until it says Done, then press play on SEQTRAK."]),
       bl(["Printed: notes, velocity, length, micro timing, track length and chords. Not printed (no SEQTRAK equivalent): probability and flam.",
           "Open it from PATTERNS → PRINT (bottom right). Afterwards set Sequencer Mode to HARDWARE so the editor doesn't play the same notes on top."])),
    ch("15. Tempo, Record Quantize and Hardware Follow", bl([
        "<b>SEND TEMPO TO SEQTRAK</b> (Connection): sends the DAW tempo (or the standalone BPM), 5–300 BPM, whenever it changes.",
        "<b>RECORD QUANTIZE:</b> leave SEQTRAK's setting or set Off, 1/32, 1/16T, 1/16, 1/8T or 1/8.",
        "<b>HARDWARE FOLLOW:</b> pressing a track button on SEQTRAK selects it in the editor, and sampler pad hits flash on the display."])),
    ch("16. Help and Mouse Tips", bl([
        "<b>HELP</b> (top bar, off by default): when on (orange), rest the pointer on any control to see what it does.",
        "Double-click a preset to load it; double-click a Deep Edit fader to reset it.",
        "Drag across the pads to paint steps; hold a pad to open its step editor.",
        "Click page numbers to change page; FOLLOW turns off so you can edit while it plays."])),
    ch("17. Troubleshooting", bl([
        "<b>Nothing happens on SEQTRAK:</b> it isn't connected yet. Follow chapter 4 (ports, then IDENTIFY). The top label must read USB MIDI.",
        "<b>IDENTIFY fails:</b> check the USB cable carries data, both ports are SEQTRAK and the device number matches. On Windows, the DAW must not be using the SEQTRAK ports.",
        "<b>No sound from the plugin track:</b> SEQTRAK makes the sound; monitor its output or its USB audio.",
        "<b>Hardware tracks are silent:</b> start SEQTRAK's sequencer, check the track is HARDWARE and MUTE ON SEQTRAK is off.",
        "<b>Stuck notes:</b> Connection → PANIC clears queued edits and sends All Sound Off.",
        "<b>Plugin missing or old version:</b> restart the DAW or rescan plugins after installing."])),
    ch("18. About and Support",
       img("About-Preview.png", 70*mm), Spacer(1, 4),
       Paragraph("Transistor Labs · ticcoross@gmail.com · https://ticcoross.gumroad.com/", B),
       Paragraph("Thanks to Jaeger (@jaeggerr), author of seqtrak-docs, for the SEQTRAK preset names and SysEx documentation. "
                 "SEQTRAK is a trademark of Yamaha Corporation; this product is not affiliated with Yamaha.", B)),
]
SimpleDocTemplate(f"{R}/docs/SEQTRAK-Editor-v{V}-User-Manual.pdf", pagesize=A4, leftMargin=20*mm, rightMargin=20*mm,
                  topMargin=18*mm, bottomMargin=18*mm, title="SEQTRAK Editor User Manual").build(story, onFirstPage=footer, onLaterPages=footer)

hist = [Paragraph("SEQTRAK Editor", T), Paragraph("Update History · Transistor Labs", S),
    ch("v0.5.2", bl(["MUTE and SOLO now also mute and solo the track on SEQTRAK itself.",
        "Print to SEQTRAK now prints the drum tracks too (they are written by step record, the method SEQTRAK accepts for drum parts).",
        "Deeper user manual: illustrated guides with numbered callouts for the panel, drawers, step editor, patterns, chain and print, concept diagrams, and detailed System Requirements."])),
    ch("v0.5.1", bl(["Step editor stays open while playing: pattern-chain or Program Change switches no longer close it; it keeps editing its own pattern (shown in its title) and the panel catches up when you close it.",
        "Pages reworked: page numbers you can click, PAGE cycles only the pages a track uses, \"PAGE x OF n\" shows the real count, FOLLOW lights orange and turns off when you pick a page.",
        "HELP button turns orange when on (it was too dark to read).",
        "REPEATER fader snaps to its 5 positions for easier mouse editing.",
        "Pattern chain: empty steps now read “(end)” (the dash showed as odd symbols).",
        "Manual: highlights how the editor powers up SEQTRAK with much more pattern memory and pattern chaining."])),
    ch("v0.5.0", bl(["64 patterns: 4 banks (A–D) × 16 slots, Program Change 0–63; older projects open in bank A.",
        "Print to SEQTRAK: write a slot into one of SEQTRAK's six patterns (drums and synths, with chords) so it plays without the computer.",
        "Step chords on Synth 1, Synth 2 and DX: 12 chord types per step, played live and printed to SEQTRAK.",
        "CHORD page connected to SEQTRAK: Mode (Mono/Poly/Chord), project Key and Scale; old Root/Type/Voicing/Spread labels removed (they don't exist on SEQTRAK).",
        "HELP button: hover help windows explaining each control (off by default).",
        "Patterns: SEND UI PC TO HOST renamed PC OUT TO DAW and HW MUTE renamed MUTE ON SEQTRAK, with tips; hardware-only controls are dimmed in PLUGIN mode.",
        "Manual: new Quick Start chapter explaining the first connection (choose the SEQTRAK USB ports, then press IDENTIFY).",
        "Pattern chain fixed: it now advances correctly in Ableton (bars were counted from a moving bar origin).",
        "Paint steps: drag across the pads to set or clear many steps at once."])),
    ch("v0.4.3", bl(["Flam per step: a softer grace note (60% velocity) placed up to half a step before the hit.",
        "Small beat marks on steps 1, 5, 9 and 13 between the pad rows to help program 4/4 patterns.",
        "Tighter timing: notes to SEQTRAK are now sent from a dedicated high-priority thread, so they stay on time when the editor or DAW is in the background or the computer is busy.",
        "Lighter editor: the panel only redraws when something changes.",
        "Double-click a preset (or press Return) to load it without pressing Load on track.",
        "Deep Edit: double-click a fader to reset it to its default value.",
        "Preset browser remembers its category, search and highlighted preset for each track.",
        "Pattern chain: up to 16 steps of slot + bars, bar-accurate switching, loop on/off, saved with the project.",
        "Orange section connected to SEQTRAK: the four knobs, FX LEVEL, HIGH PASS, REPEATER and the track volume knobs now send MIDI to the hardware; knobs with no MIDI control are dimmed."])),
    ch("v0.4.2", bl(["The selected track's name box now has a thin dark frame, so the selected channel is easy to see."])),
    ch("v0.4.1", bl(["About now opens inside the plugin window (it was hidden behind the plugin in Ableton); close it with X, Esc or a click outside.",
        "Drawers open in place without the slide that made the orange section jump."])),
    ch("v0.4.0", bl(["Hardware sequencer mode: slots switch SEQTRAK's own internal patterns (1–6) and mutes per track, with per-track mode override.",
        "22 automatable Pattern and Mute host parameters; mutes can change mid-pattern.",
        "Tempo sync to SEQTRAK, Record Quantize setting, Hardware Follow (track select and sampler pad hits).",
        "Hardware pattern number shown on the dot-matrix display. v0.3.0 banks open in PLUGIN mode."])),
    ch("v0.3.0", bl(["11-track step sequencer synced to the host (tempo, position, time signature), per-track length up to 128 steps, swing.",
        "Per-step note, velocity, probability, micro timing and gate; mute/solo affect playback.",
        "16-slot pattern bank with Program Change switching (immediate / beat / bar) and Sound Recall (none / presets / full).",
        "Notes sent to SEQTRAK over USB, optional mirroring to host MIDI.",
        "Slot and bank JSON import/export; bank saved in the project."])),
    ch("v0.2.0", bl(["USB SysEx editing ported from SEQTRAK Sound Lab: Identify, Read Hardware, Read All Tracks.",
        "Deep Edit (drum, sampler, AWM2, DX), preset loading, Track FX, randomizer with locks, undo and A/B, snapshots.",
        "Plugin is now an instrument with MIDI in/out; LED states for select/mute/solo."])),
    ch("v0.1.0", bl(["First UI build in SEQTRAK hardware style: track knobs, step pads, orange parameter section, dot-matrix display, drawers and About window."]))]
SimpleDocTemplate(f"{R}/docs/SEQTRAK-Editor-Update-History.pdf", pagesize=A4, leftMargin=20*mm, rightMargin=20*mm,
                  topMargin=18*mm, bottomMargin=18*mm, title="SEQTRAK Editor Update History").build(hist, onFirstPage=footer, onLaterPages=footer)
