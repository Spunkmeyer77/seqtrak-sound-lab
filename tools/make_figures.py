import sys, math
from PIL import Image, ImageDraw, ImageFont, ImageFilter

REPO = sys.argv[1]
OUT = "/home/claude/figs"
FB = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"
FR = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf"
def F(sz, bold=True): return ImageFont.truetype(FB if bold else FR, sz)

ORANGE = (232, 116, 59); RECESS = (217, 102, 47); DARK = (90, 38, 18); CREAM = (255, 239, 218)
INK = (39, 43, 44); GREY = (201, 204, 207); INSET = (184, 187, 190); MARK = (20, 120, 200)
S = 2  # pixels per plugin unit

def callout(d, x, y, n, r=17):
    d.ellipse((x - r, y - r, x + r, y + r), fill=MARK, outline="white", width=3)
    d.text((x, y), str(n), fill="white", font=F(int(r * 1.2)), anchor="mm")

def inpaint(im, test):
    px = im.load(); w, h = im.size
    mask = [[test(px[x, y]) for x in range(w)] for y in range(h)]
    for _ in range(12):
        changed = False
        for y in range(h):
            for x in range(w):
                if not mask[y][x]: continue
                acc = [0, 0, 0]; n = 0
                for dx, dy in ((1,0),(-1,0),(0,1),(0,-1),(2,0),(-2,0),(0,2),(0,-2)):
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < w and 0 <= yy < h and not mask[yy][xx]:
                        c = px[xx, yy]; acc[0]+=c[0]; acc[1]+=c[1]; acc[2]+=c[2]; n += 1
                if n:
                    px[x, y] = tuple(v // n for v in acc); mask[y][x] = False; changed = True
        if not changed: break

# ---------- 1. Main panel from the real screenshot ----------
im = Image.open(f"{OUT}/raw_main.png").convert("RGB").resize((2800, 840))
cyan = lambda c: c[2] > 170 and c[1] > 170 and c[0] < 130
region = im.crop((2530, 380, 2760, 840)); inpaint(region, cyan); im.paste(region, (2530, 380))
d = ImageDraw.Draw(im)
# update status line to current UI: "PAGE 1 OF 1" + page box
d.rectangle((90, 500, 440, 548), fill=(201, 204, 207))
d.text((98, 524), "HAT 2   /   PAGE 1 OF 1", fill=INK, font=F(25), anchor="lm")
d.rounded_rectangle((424, 504, 462, 542), 4, fill=INK); d.text((443, 523), "1", fill=CREAM, font=F(22), anchor="mm")
for n, (x, y) in enumerate([(458, 24), (16, 108), (190, 108), (252, 262), (560, 345), (96, 345),
                             (958, 232), (1047, 150), (1152, 182), (1166, 42), (1256, 214),
                             (843, 396), (1384, 18)], 1):
    callout(d, x * S, y * S, n, 30)
im.save(f"{OUT}/fig_main.png")

# ---------- helpers for drawer schematics ----------
def drawer(title, elements, callouts, size=(525, 378), name="x", extra=None):
    w, h = size
    im = Image.new("RGB", (w * S, h * S), ORANGE); d = ImageDraw.Draw(im)
    d.text((15 * S, 25 * S), title, fill=CREAM, font=F(36), anchor="lm")
    for e in elements:
        kind, (x, y, ew, eh), text = e[0], e[1], e[2]
        on = len(e) > 3 and e[3]
        box = (x * S, y * S, (x + ew) * S, (y + eh) * S)
        if kind == "btn":
            d.rounded_rectangle(box, 4, fill=DARK if on else RECESS)
            d.text(((x + ew / 2) * S, (y + eh / 2) * S), text, fill=CREAM, font=F(min(22, int(eh * 0.85))), anchor="mm")
        elif kind == "combo":
            d.rounded_rectangle(box, 4, fill=CREAM, outline=DARK, width=2)
            d.text(((x + 6) * S, (y + eh / 2) * S), text, fill=INK, font=F(min(22, int(eh * 0.8)), False), anchor="lm")
            cx, cy = (x + ew - 12) * S, (y + eh / 2) * S
            d.line((cx - 9, cy - 4, cx, cy + 5, cx + 9, cy - 4), fill=INK, width=3)
        elif kind == "label":
            d.text((x * S, (y + eh / 2) * S), text, fill=DARK if not on else CREAM, font=F(20), anchor="lm")
        elif kind == "slider":
            d.text((x * S, (y + eh / 2) * S), text, fill=DARK, font=F(20), anchor="lm")
            sx0, sx1, sy = (x + 165) * S, (x + 420) * S, (y + eh / 2) * S
            d.line((sx0, sy, sx1, sy), fill=DARK, width=6)
            k = sx0 + (sx1 - sx0) * (e[3] if len(e) > 3 else 0.5)
            d.ellipse((k - 12, sy - 12, k + 12, sy + 12), fill=CREAM)
            d.rounded_rectangle(((x + 432) * S, (y + 4) * S, (x + 488) * S, (y + eh - 4) * S), 4, fill=CREAM)
            d.text(((x + 460) * S, sy), e[4] if len(e) > 4 else "", fill=INK, font=F(20, False), anchor="mm")
    if extra: extra(d)
    for n, (x, y) in enumerate(callouts, 1): callout(d, x * S, y * S, n)
    im.save(f"{OUT}/fig_{name}.png")

# ---------- 2. Step editor ----------
drawer("SYNTH 1  /  A01  /  STEP 5", [
    ("btn", (438, 14, 70, 26), "CLOSE"),
    ("btn", (16, 59, 103, 28), "ACTIVE", True), ("label", (136, 59, 60, 28), "CHORD"),
    ("combo", (196, 59, 150, 28), "Minor"),
    ("slider", (16, 98, 490, 35), "VELOCITY", 0.78, "100"),
    ("slider", (16, 141, 490, 35), "MIDI NOTE", 0.47, "60"),
    ("slider", (16, 184, 490, 35), "PROBABILITY (%)", 1.0, "100"),
    ("slider", (16, 227, 490, 35), "MICRO TIMING", 0.5, "0"),
    ("slider", (16, 270, 490, 35), "GATE (% STEP)", 0.5, "50"),
    ("slider", (16, 313, 490, 35), "FLAM (% STEP)", 0.3, "15"),
], [(8, 73), (350, 73), (174, 115), (174, 158), (174, 201), (174, 244), (174, 287), (174, 330)], name="step")

# ---------- 3. Patterns drawer ----------
slots = [("btn", (16 + (k % 8) * 62, 72 + (k // 8) * 40, 56, 34), "B%02d" % (k + 1), k == 2) for k in range(16)]
drawer("PATTERNS  /  KICK", [
    ("btn", (240, 12, 66, 24), "CHAIN"), ("label", (325, 12, 120, 24), "USB MIDI"), ("btn", (462, 12, 49, 24), "CLOSE"),
    ("label", (16, 45, 290, 23), "ACTIVE B03  /  Verse"),
    *[("btn", (309 + b * 50, 44, 46, 24), "ABCD"[b], b == 1) for b in range(4)],
    *slots,
    ("combo", (16, 154, 182, 25), "Verse"), ("combo", (204, 154, 204, 25), "Verse"), ("btn", (414, 154, 95, 25), "RENAME"),
    ("btn", (16, 186, 116, 25), "COPY"), ("btn", (140, 186, 116, 25), "PASTE"), ("btn", (264, 186, 116, 25), "CLEAR"),
    ("btn", (388, 186, 121, 25), "SWITCH"),
    ("label", (16, 217, 150, 20), "SWITCH QUANTIZE"), ("label", (172, 217, 150, 20), "SOUND RECALL"),
    ("combo", (16, 240, 147, 26), "Next bar"), ("combo", (172, 240, 147, 26), "None"), ("btn", (328, 240, 181, 26), "PC OUT TO DAW"),
    *[("btn", (16 + i * 124, 280, 121, 27), t) for i, t in enumerate(["IMPORT SLOT", "EXPORT SLOT", "IMPORT BANK", "EXPORT BANK"])],
    ("label", (16, 311, 125, 16), "SEQUENCER MODE"), ("label", (147, 311, 135, 16), "TRACK OVERRIDE"), ("label", (287, 311, 120, 16), "SEQTRAK PATTERN"),
    ("combo", (16, 329, 125, 24), "PLUGIN"), ("combo", (147, 329, 134, 24), "Follow mode"), ("combo", (287, 329, 105, 24), "1"),
    ("btn", (400, 329, 109, 24), "MUTE ON SEQTRAK"),
    ("label", (16, 354, 380, 22), "Slot switches at the next bar."), ("btn", (400, 352, 109, 24), "PRINT"),
], [(232, 24), (300, 56), (8, 92), (8, 166), (8, 198), (8, 253), (322, 253), (8, 293), (8, 341), (143, 341), (396, 341), (396, 364)], name="patterns")

# ---------- 4. Chain page ----------
rows = []
demo = [("A01  Intro", 2), ("A02  Verse", 4), ("B03  Chorus", 4), ("A02  Verse", 4)]
for i in range(16):
    x, y = 16 + (i // 8) * 250, 106 + (i % 8) * 33
    rows.append(("label", (x, y, 26, 26), str(i + 1), i == 1))
    txt, bars = demo[i] if i < len(demo) else ("(end)", 1)
    rows.append(("combo", (x + 28, y, 130, 26), txt)); rows.append(("combo", (x + 162, y, 78, 26), "%d bar%s" % (bars, "" if bars == 1 else "s")))
drawer("PATTERN CHAIN", [("btn", (16, 48, 104, 26), "CHAIN ON", True), ("btn", (128, 48, 80, 26), "LOOP", True),
    ("btn", (216, 48, 80, 26), "CLEAR"), ("btn", (409, 48, 100, 26), "PATTERNS"), ("btn", (462, 12, 49, 24), "CLOSE"),
    ("label", (16, 80, 493, 20), "PLAYING STEP 2 OF 4  /  SLOT A02", True), *rows],
    [(10, 61), (124, 61), (212, 61), (8, 90), (150, 102), (238, 102), (8, 238)], name="chain")

# ---------- 5. Print page ----------
tr = ["KICK", "SNARE", "CLAP", "HAT 1", "HAT 2", "PERC 1", "PERC 2", "SYNTH 1", "SYNTH 2", "DX", "SAMPLER"]
drawer("PRINT TO SEQTRAK", [("btn", (462, 12, 49, 24), "CLOSE"),
    ("label", (16, 44, 493, 20), "Writes a slot into one of SEQTRAK's own patterns."),
    ("label", (16, 72, 110, 26), "FROM SLOT"), ("combo", (128, 72, 220, 26), "B03  Chorus"),
    ("label", (16, 104, 150, 26), "TO SEQTRAK PATTERN"), ("combo", (168, 104, 80, 26), "4"),
    ("btn", (409, 72, 100, 26), "PATTERNS"), ("label", (16, 138, 120, 20), "TRACKS"),
    *[("btn", (16 + (t % 6) * 82, 160 + (t // 6) * 30, 78, 26), tr[t], True) for t in range(11)],
    ("btn", (427, 190, 82, 26), "ALL"),
    ("label", (16, 230, 493, 30), "Prints notes, velocity, length, micro timing and chords."),
    ("btn", (16, 272, 150, 30), "PRINT TO SEQTRAK"), ("label", (176, 272, 333, 30), "Done. Press play on SEQTRAK.", True),
], [(354, 85), (256, 117), (8, 148), (8, 287), (172, 300)], name="print")

# ---------- 6. Connection drawer ----------
drawer("CONNECTION  /  KICK", [("btn", (462, 12, 49, 24), "CLOSE"),
    ("label", (16, 46, 195, 22), "MIDI INPUT"), ("label", (218, 46, 195, 22), "MIDI OUTPUT"), ("label", (420, 46, 90, 22), "DEVICE"),
    ("combo", (16, 73, 195, 29), "SEQTRAK"), ("combo", (218, 73, 195, 29), "SEQTRAK"), ("combo", (420, 73, 89, 29), "1"),
    ("btn", (16, 114, 105, 29), "IDENTIFY", True), ("btn", (128, 114, 120, 29), "READ HARDWARE"),
    ("btn", (255, 114, 119, 29), "READ ALL TRACKS"), ("btn", (381, 114, 128, 29), "DISCONNECT"),
    ("btn", (16, 150, 105, 25), "REFRESH PORTS"), ("btn", (128, 150, 120, 25), "PANIC"),
    ("btn", (255, 150, 254, 25), "MIRROR NOTES TO HOST MIDI"),
    ("label", (16, 190, 493, 40), "SEQTRAK found. Editing enabled.", True),
    ("btn", (16, 323, 172, 25), "SEND TEMPO TO SEQTRAK"), ("btn", (194, 323, 131, 25), "HARDWARE FOLLOW", True),
    ("label", (332, 305, 176, 16), "RECORD QUANTIZE"), ("combo", (332, 323, 177, 25), "Leave unchanged"),
], [(8, 87), (214, 87), (416, 87), (8, 128), (252, 128), (8, 162), (251, 162), (8, 210), (8, 336), (328, 336)], name="connection")

# ---------- 7. Concept diagrams ----------
def canvas(w, h): im = Image.new("RGB", (w, h), "white"); return im, ImageDraw.Draw(im)
def box(d, xy, text, fill=ORANGE, fg=CREAM, sz=26):
    d.rounded_rectangle(xy, 14, fill=fill)
    d.multiline_text(((xy[0] + xy[2]) / 2, (xy[1] + xy[3]) / 2), text, fill=fg, font=F(sz), anchor="mm", align="center", spacing=6)
def arrow(d, a, b, text="", col=INK):
    d.line((a, b), fill=col, width=5)
    ang = math.atan2(b[1] - a[1], b[0] - a[0])
    for s in (2.6, -2.6):
        d.line((b, (b[0] - 22 * math.cos(ang + s * 0.2), b[1] - 22 * math.sin(ang + s * 0.2))), fill=col, width=5)
    if text: d.text(((a[0] + b[0]) / 2, (a[1] + b[1]) / 2 - 22), text, fill=col, font=F(21), anchor="mm")

# connection
im, d = canvas(1600, 520)
box(d, (40, 120, 520, 400), "COMPUTER\nAbleton Live / DAW\nSEQTRAK Editor plugin", INK, CREAM, 30)
box(d, (1080, 120, 1560, 400), "YAMAHA SEQTRAK\nsounds, effects,\nits own sequencer", ORANGE, CREAM, 30)
arrow(d, (540, 220), (1060, 220), "USB MIDI: SysEx edits, notes, CC")
arrow(d, (1060, 310), (540, 310), "replies: identity, sounds, panel")
d.text((800, 450), "USB data cable  ·  choose the SEQTRAK ports in CONNECTION, then press IDENTIFY", fill=INK, font=F(23), anchor="mm")
d.text((800, 60), "How the editor talks to SEQTRAK", fill=INK, font=F(34), anchor="mm")
im.save(f"{OUT}/fig_connect.png")

# memory
im, d = canvas(1600, 640)
d.text((800, 45), "Pattern memory: SEQTRAK vs SEQTRAK Editor", fill=INK, font=F(34), anchor="mm")
d.text((260, 110), "SEQTRAK alone", fill=INK, font=F(28), anchor="mm")
for i in range(6): box(d, (90 + (i % 3) * 120, 150 + (i // 3) * 120, 190 + (i % 3) * 120, 250 + (i // 3) * 120), str(i + 1), INSET, INK, 30)
d.text((260, 420), "6 patterns per track", fill=INK, font=F(24), anchor="mm")
d.text((1080, 110), "With SEQTRAK Editor", fill=INK, font=F(28), anchor="mm")
for b in range(4):
    d.text((620, 165 + b * 100), "ABCD"[b], fill=INK, font=F(30), anchor="mm")
    for k in range(16):
        x = 650 + k * 54; y = 140 + b * 100
        d.rounded_rectangle((x, y, x + 46, y + 50), 6, fill=ORANGE if b == 1 and k == 2 else RECESS)
d.text((1080, 560), "64 patterns: 4 banks × 16", fill=INK, font=F(26), anchor="mm"); d.text((1080, 600), "switch by click, Program Change 0–63 or Pattern Chain", fill=INK, font=F(22), anchor="mm")
d.text((260, 480), "Print any of them\nonto SEQTRAK's 6 slots", fill=DARK, font=F(22), anchor="mm", align="center")
im.save(f"{OUT}/fig_memory.png")

# modes
im, d = canvas(1600, 560)
d.text((800, 40), "Sequencer Mode: who plays the notes?", fill=INK, font=F(34), anchor="mm")
box(d, (40, 100, 760, 200), "PLUGIN mode", INK, CREAM, 30)
box(d, (60, 240, 360, 360), "Editor\nsequencer", ORANGE, CREAM, 26); box(d, (440, 240, 740, 360), "SEQTRAK\nsounds", INSET, INK, 26)
arrow(d, (370, 300), (430, 300)); d.text((400, 420), "Notes come from the editor (steps, flam,\nprobability, chords). Mutes: left strip.", fill=INK, font=F(22), anchor="mm", align="center")
box(d, (840, 100, 1560, 200), "HARDWARE mode", INK, CREAM, 30)
box(d, (860, 240, 1160, 360), "Editor\nswitches", ORANGE, CREAM, 26); box(d, (1240, 240, 1540, 360), "SEQTRAK's own\nsequencer", INSET, INK, 26)
arrow(d, (1170, 300), (1230, 300)); d.text((1200, 420), "Each slot picks SEQTRAK pattern 1–6 and\nMUTE ON SEQTRAK per track; SEQTRAK plays.", fill=INK, font=F(22), anchor="mm", align="center")
im.save(f"{OUT}/fig_modes.png")

# chain timeline
im, d = canvas(1600, 380)
d.text((800, 40), "Pattern Chain: steps play in order, each for its bars, switching on the bar", fill=INK, font=F(30), anchor="mm")
x = 60; cols = [ORANGE, RECESS, DARK, RECESS]
for (name, bars), c in zip(demo, cols):
    w = bars * 90
    box(d, (x, 110, x + w - 8, 220), name.split("  ")[0] + "\n" + name.split("  ")[1] + f"\n{bars} bars", c, CREAM, 22); x += w
arrow(d, (x + 10, 165), (x + 80, 165)); d.text((x + 120, 165), "LOOP\nto 1", fill=INK, font=F(22), anchor="mm", align="center")
for b in range(15): d.line((60 + b * 90, 240, 60 + b * 90, 260), fill=INK, width=3)
d.text((800, 300), "bars", fill=INK, font=F(22), anchor="mm")
im.save(f"{OUT}/fig_chaintime.png")

# flam & chord
im, d = canvas(1600, 420)
d.text((400, 40), "Flam", fill=INK, font=F(32), anchor="mm"); d.text((1200, 40), "Step chord (Minor on C)", fill=INK, font=F(32), anchor="mm")
d.line((80, 300, 720, 300), fill=INK, width=4)
for s in range(5): d.line((80 + s * 160, 290, 80 + s * 160, 310), fill=INK, width=3)
d.rectangle((300, 160, 324, 300), fill=RECESS); d.text((312, 140), "grace\n60%", fill=DARK, font=F(20), anchor="mm", align="center")
d.rectangle((400, 100, 430, 300), fill=ORANGE); d.text((415, 80), "hit 100%", fill=DARK, font=F(20), anchor="mm")
arrow(d, (312, 340), (408, 340), col=DARK); d.text((360, 375), "FLAM % of a step", fill=DARK, font=F(20), anchor="mm")
for i, (n, semi) in enumerate([("C", 0), ("Eb", 3), ("G", 7)]):
    y = 300 - semi * 22
    d.rounded_rectangle((1000, y - 18, 1400, y + 18), 8, fill=ORANGE if i == 0 else RECESS)
    d.text((1200, y), f"{n}  (root{'' if i == 0 else ' + %d' % semi})", fill=CREAM, font=F(22), anchor="mm")
d.text((1200, 360), "All notes start together; PRINT writes every note", fill=INK, font=F(21), anchor="mm")
im.save(f"{OUT}/fig_flamchord.png")

# preset & deep edit drawers from the clean previews (orange half)
for src, name in [("Preset-Drawer-Preview.png", "presets"), ("DX-Drawer-Preview.png", "deep")]:
    p = Image.open(f"{REPO}/{src}").convert("RGB").crop((841, 0, 1366, 378)).resize((1050, 756), Image.LANCZOS)
    d = ImageDraw.Draw(p)
    pts = {"presets": [(8, 63), (287, 63), (8, 110), (8, 327), (359, 355)],
           "deep": [(8, 63), (126, 63), (236, 26), (8, 120), (140, 120), (252, 120)]}[name]
    for n, (x, y) in enumerate(pts, 1): callout(d, x * S, y * S, n)
    p.save(f"{OUT}/fig_{name}.png")
print("ok")
