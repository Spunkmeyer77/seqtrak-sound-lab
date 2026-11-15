#include <juce_audio_utils/juce_audio_utils.h>
#include <BinaryData.h>
#include "Hardware.h"
#include "Sequencer.h"
#include "PatternJson.h"
#include <array>
#include <cmath>
#include <vector>

namespace
{
using namespace juce;
constexpr int width = 1400, height = 420;
const Colour grey (0xffc9cccf), inset (0xffb8bbbe), orange (0xffe8743b),
             recess (0xffd9662f), ink (0xff272b2c), cream (0xffffefda),
             darkOrange (0xff552713), amber (0xffffbf60), green (0xffbdf482), purple (0xffcbaaff);
const StringArray tracks { "KICK", "SNARE", "CLAP", "HAT 1", "HAT 2", "PERC 1",
                           "PERC 2", "SYNTH 1", "SYNTH 2", "DX", "SAMPLER" };
const StringArray sections { "SOUND", "PITCH", "MONO/POLY", "CHORD", "PAN", "VOLUME",
    "ATTACK", "DECAY", "MICRO TIMING", "FILTER", "RESONANCE", "REVERB", "DELAY",
    "COMP", "DISTORTION", "MODULATION", "DUCKER", "OTHER", "MIXER", "SONG" };
const StringArray drawers { "PRESETS", "DEEP EDIT", "TRACK FX", "RANDOM", "PATTERNS", "CONNECTION" };
const std::array<StringArray, 20> knobLabels {{
    {"Preset", "Bank", "Category", "Variation"}, {"Coarse", "Fine", "Glide", "Bend"},
    {"Mode", "Legato", "Glide", "Priority"}, {"Mode", "Key", "Scale"},
    {"Pan"}, {"Volume"}, {"Attack"}, {"Decay", "Sustain", "Release"},
    {"Offset", "Velocity", "Probability", "Note"}, {"Cutoff", "EG Depth", "Attack", "Decay"},
    {"Resonance"}, {"Send", "Time", "Damping", "Width"}, {"Send", "Time", "Feedback", "Tone"},
    {"Threshold", "Ratio", "Attack", "Release"}, {"Drive", "Tone", "Mix", "Level"},
    {"Rate", "Depth", "Feedback", "Mix"}, {"Amount", "Attack", "Hold", "Release"},
    {"Level", "Accent", "Length", "Offset"}, {"Volume", "Pan", "Reverb", "Delay"},
    {"Pattern", "Length", "Repeat", "Swing"}
}};

// MIDI Control Change for Sound Design knob `knob` in panel section `section`
// on track `track` (0-10), from the SEQTRAK Data List CC map. -1 = no CC exists
// (those knobs are dimmed; use Deep Edit for them).
constexpr int keyControl = 1000, scaleControl = 1001;   // not CCs: sent as SysEx parameter changes

int quickControl (int section, int knob, int track)
{
    const bool drum = track < 7, synth = track >= 7 && track <= 9;
    switch (section)
    {
        case 1:  if (knob == 0 && drum) return 25; if (knob == 2 && synth) return 5; return -1;   // PITCH: drum pitch, glide
        case 2:  return knob == 0 && synth ? 26 : -1;                                             // MONO/POLY/CHORD
        case 3:  if (knob == 0) return synth ? 26 : -1;                                           // CHORD: mode (2 = chord)
                 if (knob == 1) return keyControl;                                                // project key (SysEx)
                 if (knob == 2) return scaleControl;                                              // project scale (SysEx)
                 return -1;
        case 4:  return knob == 0 ? 10 : -1;                                                      // PAN
        case 5:  return knob == 0 ? 7 : -1;                                                       // VOLUME
        case 6:  return knob == 0 ? 73 : -1;                                                      // ATTACK
        case 7:  return knob == 0 ? 75 : -1;                                                      // DECAY / RELEASE
        case 9:  return knob == 0 ? 74 : -1;                                                      // FILTER cutoff
        case 10: return knob == 0 ? 71 : -1;                                                      // RESONANCE
        case 11: return knob == 0 ? 91 : -1;                                                      // REVERB send
        case 12: return knob == 0 ? 94 : -1;                                                      // DELAY send
        case 18: { const int mix[] { 7, 10, 91, 94 }; return mix[knob]; }                        // MIXER
        default: return -1;
    }
}

// Hover help shown when HELP is on, keyed by control text.
String helpFor (const String& text)
{
    static const std::map<String, String> help {
        { "HELP", "Turn hover help on or off." },
        { "ABOUT", "Version, contact and website." },
        { "FOLLOW", "Step pads follow the playing page." },
        { "VOL+", "Raise the selected track's volume." }, { "VOL-", "Lower the selected track's volume." },
        { "DELETE", "Clear all steps of the selected track." },
        { "MUTE", "Mute the selected track in the plugin sequencer." },
        { "SOLO", "Solo the selected track in the plugin sequencer." },
        { "PAGE", "Next page of 16 steps (up to 8 pages)." },
        { "PLAY", "Start or stop the sequencer (standalone; in a DAW use the DAW transport)." },
        { "USB", "Open Connection: choose SEQTRAK's USB ports and press IDENTIFY." },
        { "MIDI", "Open Connection: MIDI ports and options." },
        { "BPM+", "Raise tempo (standalone)." }, { "BPM-", "Lower tempo (standalone)." },
        { "SWING", "Set swing, 50-75%." }, { "PROJECT", "Open the Patterns drawer." },
        { "PRESETS", "Browse and load SEQTRAK sounds. Double-click to load." },
        { "DEEP EDIT", "Edit every sound parameter. Double-click a fader to reset it." },
        { "TRACK FX", "Read, choose and edit the track's effect." },
        { "RANDOM", "Randomize the track's sound; lock what you want to keep." },
        { "PATTERNS", "16 pattern slots, Program Change switching, chain, import/export." },
        { "CONNECTION", "MIDI ports, IDENTIFY, reading hardware, tempo and options." },
        { "CLOSE", "Close this drawer." }, { "SNAPS", "Save and recall voice snapshots." },
        { "CHAIN", "Pattern chain: play slots in order, each for a number of bars." },
        { "CHAIN ON", "Run the chain when playback starts." },
        { "LOOP", "Repeat the chain from step 1 at the end." },
        { "COPY", "Copy the selected slot." }, { "PASTE", "Paste into the selected slot." },
        { "CLEAR", "Clear the selected slot (or the chain)." }, { "SWITCH", "Switch to the selected slot." },
        { "RENAME", "Rename the selected slot." },
        { "IMPORT SLOT", "Load one slot from a JSON file." }, { "EXPORT SLOT", "Save one slot as a JSON file." },
        { "IMPORT BANK", "Load all 16 slots from a JSON file." }, { "EXPORT BANK", "Save all 16 slots as a JSON file." },
        { "READ HARDWARE", "Read this track's sound from SEQTRAK." },
        { "HOLD TO AUDITION", "Hold to play a test note on SEQTRAK." },
        { "LOCK", "Keep this parameter when randomizing." },
        { "LOAD ON TRACK", "Load the highlighted sound on the selected track." },
        { "LOAD ON PAD", "Load the highlighted sample on the chosen pad." },
        { "ACTIVE", "Turn this step on or off." },
        { "MASTER", "FX LEVEL controls the master effect. Click for SINGLE." },
        { "SINGLE", "FX LEVEL controls the selected track's effect. Click for MASTER." },
        { "ALL", "Mixer page for all tracks." },
    };
    if (const auto it = help.find (text); it != help.end()) return it->second;
    if (text.startsWith ("LENGTH")) return "Set the selected track's length in steps (1-128).";
    if (tracks.contains (text)) return "Select this track. The knob above sets its volume on SEQTRAK.";
    if (sections.contains (text)) return "Show this parameter page on the four orange knobs.";
    return {};
}

Font face (float size, int flags = Font::plain, float scale = 0.9f)
{
    auto font = Font (FontOptions (size, flags));
    return font.withHorizontalScale (scale);
}

var parse (const char* bytes, int size) { return JSON::parse (String::fromUTF8 (bytes, size)); }

class PanelLook final : public LookAndFeel_V4
{
public:
    PanelLook()
    {
        setColour (Slider::thumbColourId, cream);
        setColour (Slider::trackColourId, darkOrange);
        setColour (Slider::backgroundColourId, recess);
        setColour (TextEditor::backgroundColourId, cream);
        setColour (TextEditor::textColourId, ink);
        setColour (TextEditor::outlineColourId, darkOrange);
        setColour (ComboBox::backgroundColourId, cream);
        setColour (ComboBox::textColourId, ink);
        setColour (ComboBox::arrowColourId, ink);
        setColour (ComboBox::outlineColourId, darkOrange);
        setColour (ListBox::backgroundColourId, recess);
        setColour (ScrollBar::thumbColourId, darkOrange);
        setColour (Slider::textBoxTextColourId, darkOrange);
        setColour (Slider::textBoxBackgroundColourId, cream);
        setColour (Slider::textBoxOutlineColourId, recess);
    }

    Font getTextButtonFont (TextButton& b, int h) override
    {
        return face (h > 22 ? 13.0f : 11.0f,
                     Font::bold | (b.getProperties().contains ("italic") ? Font::italic : Font::plain));
    }
    Font getComboBoxFont (ComboBox&) override { return face (14); }

    void drawButtonBackground (Graphics& g, Button& b, const Colour& base, bool hover, bool down) override
    {
        auto colour = b.getToggleState() ? b.findColour (TextButton::buttonOnColourId) : base;
        if (b.getProperties().contains ("italic"))
            colour = b.getToggleState() ? grey.brighter (0.16f) : inset;
        if (hover) colour = colour.brighter (0.12f);
        if (down) colour = colour.darker (0.2f);
        g.setColour (colour.withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.65f));
        g.fillRoundedRectangle (b.getLocalBounds().toFloat().reduced (0.5f), 2.0f);
        if (b.getProperties().contains ("italic") && b.getToggleState())
        {
            // Selected track: thin dark frame around the name box.
            g.setColour (ink);
            g.drawRoundedRectangle (b.getLocalBounds().toFloat().reduced (1.0f), 2.0f, 1.5f);
        }
    }

    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float value,
                           float start, float end, Slider& s) override
    {
        const auto area = Rectangle<float> (float (x), float (y), float (w), float (h));
        auto centre = area.getCentre();
        const float r = jmin (area.getWidth(), area.getHeight()) * 0.37f;
        auto disc = Rectangle<float> (r * 2, r * 2).withCentre (centre);
        const bool warm = s.getProperties().contains ("orange");
        g.setColour (warm ? darkOrange.withAlpha (0.45f) : ink.withAlpha (0.29f));
        g.fillEllipse (disc.expanded (1.5f));
        g.setColour (warm ? Colour (0xfff78a4c) : Colour (0xfff5f5f2));
        g.fillEllipse (disc);
        const auto angle = start + value * (end - start);
        const Point<float> direction (std::sin (angle), -std::cos (angle));
        g.setColour (warm ? darkOrange : Colour (0xff696f71));
        g.drawLine ({ centre + direction * (r * 0.58f), centre + direction * (r * 0.87f) }, 2.1f);
    }

    void drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos,
                           float min, float max, Slider::SliderStyle style, Slider& s) override
    {
        if (!s.getProperties().contains ("ladder"))
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, min, max, style, s);
            return;
        }
        const auto cx = float (x) + float (w) * 0.5f;
        for (int i = 0; i != 9; ++i)
        {
            const auto yy = float (y + 6) + float (h - 12) * float (i) / 8.0f;
            g.setColour (darkOrange.withAlpha (0.65f));
            g.drawHorizontalLine (roundToInt (yy), cx - 19.0f, cx + 19.0f);
        }
        g.setColour (cream);
        g.fillRoundedRectangle (cx - 14.0f, pos - 3.0f, 28.0f, 6.0f, 1.0f);
    }
};

class Processor final : public AudioProcessor, private Timer
{
public:
    Processor() : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
                  state (*this, nullptr, "SEQTRAK_EDITOR", parameters()),
                  hardware (parse (BinaryData::VoiceSchema_json, BinaryData::VoiceSchema_jsonSize),
                            parse (BinaryData::Effects_json, BinaryData::Effects_jsonSize), tracks),
                  voiceSchema (parse (BinaryData::VoiceSchema_json, BinaryData::VoiceSchema_jsonSize))
    {
        for (int t = 0; t < 11; ++t)
        {
            patternParameters[size_t (t)] = state.getRawParameterValue ("hwPattern" + String (t));
            volumeParameters[size_t (t)] = state.getRawParameterValue ("vol" + String (t));
            muteParameters[size_t (t)] = state.getRawParameterValue ("hwMute" + String (t));
        }
        for (int k = 0; k < 4; ++k) quickParameters[size_t (k)] = state.getRawParameterValue ("quick" + String (k));
        fxLevelParameter = state.getRawParameterValue ("fxlevel");
        highPassParameter = state.getRawParameterValue ("highpass");
        repeatParameter = state.getRawParameterValue ("repeat");
        hardware.onTrackSelected = [this] (int t) { if (bank.hardwareFollow.load()) bank.selectedTrack.store (t); };
        hardware.onSamplerPad = [this] (int pad) { if (bank.hardwareFollow.load()) bank.samplerPad.store (pad); };
        hardware.onProgramChange = [this] (int program) { bank.hardwareProgram.store (program); };
        hardware.onPresetLoad = [this] (int part, int number, int pad)
        {
            const std::lock_guard<std::mutex> guard (bank.lock);
            auto& p = bank.slots[size_t (bank.current.load())];
            p.sounds[size_t (part)] = {};
            if (part == 10) p.samples[size_t (pad)] = number;
            else p.sounds[size_t (part)].number = number;
        };
        hardware.onSequenceTick = [this] { syncSounds(); };
        startTimerHz (30);
    }

    ~Processor() override { stopTimer(); hardware.onSequenceTick = {}; }

    void timerCallback() override { syncSounds(); }

    void syncSounds()
    {
        const auto switched = bank.switched.load();
        const int slot = bank.current.load();
        const bool changed = switched != lastSwitch;
        lastSwitch = switched;
        if (changed && hardware.verified && bank.soundMode.load() != 0)
        {
            const std::lock_guard<std::mutex> guard (bank.lock);
            const auto& pattern = bank.slots[size_t (slot)];
            hardware.beginRecall();
            const bool full = bank.soundMode.load() == 2;
            for (int part = 0; part < 10; ++part)
                hardware.recallSound (part, pattern.sounds[size_t (part)].number, {}, -1, {}, false);
            for (int pad = 0; pad < 7; ++pad)
                hardware.recallSound (10, pattern.samples[size_t (pad)], {}, -1, {}, false, pad);
            if (full)
                for (int part = 0; part < 11; ++part)
                {
                    const auto& sound = pattern.sounds[size_t (part)];
                    hardware.recallSound (part, 0, sound.voice, sound.effect, sound.fx, true);
                }
        }
        if (changed)
        {
            hardwareRevision = hardware.revision;
            for (int part = 0; part < 11; ++part)
                soundRevisions[size_t (part)] = hardware.tracks[size_t (part)].revision;
        }
        if (!hardware.verified || hardwareRevision == hardware.revision) return;
        hardwareRevision = hardware.revision;
        const std::lock_guard<std::mutex> guard (bank.lock);
        if (bank.current.load() != slot) return;
        auto& pattern = bank.slots[size_t (slot)];
        for (int part = 0; part < 11; ++part)
        {
            const auto& read = hardware.tracks[size_t (part)];
            if (read.revision == soundRevisions[size_t (part)]) continue;
            soundRevisions[size_t (part)] = read.revision;
            auto& sound = pattern.sounds[size_t (part)];
            if (read.number > 0 && read.number <= 2032 && part < 10 && sound.number != read.number)
            {
                sound = {};
                sound.number = read.number;
            }
            if (read.name.isNotEmpty()) sound.name = read.name;
            if (!read.known.empty()) sound.voice = read.known;
            if (!hardware.effectFor (read.effect).isVoid())
            {
                sound.effect = read.effect;
                sound.fx = read.fx;
            }
        }
    }

    const String getName() const override { return "SEQTRAK Editor"; }
    void prepareToPlay (double rate, int) override
    {
        sampleRate = rate;
        sequencer.reset (bank.current.load(), bank.editGeneration.load());
        standalonePpq = 0;
        controlSlot = sentTempo = sentRecordQuantize = -1;
        controlExpected = 0;
        controlWasPlaying = false;
        generated.ensureSize (65536);
    }
    void releaseResources() override
    {
        hardware.cancelScheduledNotes();
        sequencer.flush (0, [this] (bool on, int t, int n, int v, int, bool, bool usb)
        {
            if (usb) hardware.scheduleNote (on, t, n, v, Time::getMillisecondCounterHiRes());
        });
        bank.playing.store (false);
    }
    bool isBusesLayoutSupported (const BusesLayout& layout) const override
    {
        return layout.getMainOutputChannelSet() == AudioChannelSet::stereo()
            && layout.getMainInputChannelSet().isDisabled();
    }
    void processBlock (AudioBuffer<float>& audio, MidiBuffer& midi) override
    {
        audio.clear();
        const int count = audio.getNumSamples();
        if (sampleRate <= 0 || count == 0) { midi.clear(); return; }
        if (audioRevision != bank.sequenceRevision.load())
        {
            const std::unique_lock<std::mutex> guard (bank.lock, std::try_to_lock);
            if (guard.owns_lock())
            {
                for (int i = 0; i < 16; ++i) audioSequences[size_t (i)] = bank.slots[size_t (i)].sequence;
                audioRevision = bank.sequenceRevision.load();
            }
            else if (audioRevision == 0 || audioGeneration != bank.editGeneration.load())
            {
                midi.clear();
                return;
            }
        }
        audioGeneration = bank.editGeneration.load();
        seqtrak::Clock clock;
        bool hasHostTempo = false;
        clock.bpm = bank.tempo.load();
        if (wrapperType == wrapperType_Standalone)
        {
            clock.playing = bank.standalonePlay.load();
            if (!clock.playing) standalonePpq = 0;
            clock.ppq = standalonePpq;
            if (clock.playing) standalonePpq += count * clock.bpm / (60.0 * sampleRate);
        }
        else if (auto* playhead = getPlayHead())
        {
            if (const auto pos = playhead->getPosition())
            {
                const auto ppq = pos->getPpqPosition();
                clock.playing = pos->getIsPlaying() && bool (ppq);
                clock.ppq = ppq.orFallback (0.0);
                if (const auto tempo = pos->getBpm(); tempo && std::isfinite (*tempo) && *tempo > 0)
                {
                    clock.bpm = *tempo;
                    hasHostTempo = true;
                }
                const auto signature = pos->getTimeSignature();
                if (signature && signature->numerator > 0 && signature->denominator > 0)
                {
                    clock.beatLength = 4.0 / signature->denominator;
                    clock.barLength = signature->numerator * clock.beatLength;
                }
                clock.barOrigin = pos->getPpqPositionOfLastBarStart().orFallback (0.0);
            }
        }
        const auto blockTime = Time::getMillisecondCounterHiRes() + 4.0;
        generated.clear();
        const auto epoch = hardware.connectionEpoch();
        if (controlEpoch != epoch)
        {
            controlEpoch = epoch;
            controlSlot = -1;
            sentTempo = sentRecordQuantize = -1;
        }
        const auto increment = clock.bpm / (60.0 * sampleRate);
        const bool transportJump = !clock.playing || !controlWasPlaying
            || std::abs (clock.ppq - controlExpected) > increment * 2.0;
        const int switchTiming = bank.quantize.load();
        const bool restartControls = controlGeneration != audioGeneration || transportJump || controlTiming != switchTiming;
        controlTiming = switchTiming;
        if (controlGeneration != audioGeneration) controlSlot = -1;
        if (controlGeneration != audioGeneration || (transportJump && controlWasPlaying))
        {
            for (auto& control : trackControls)
                if (control.enabled) control.pending = control.force = true;
        }
        controlGeneration = audioGeneration;
        controlExpected = clock.ppq + count * increment;
        controlWasPlaying = clock.playing;
        int ui = bank.requested.exchange (-1);
        const int usb = bank.hardwareProgram.exchange (-1);
        const int chained = runChain (clock, count * increment);
        const bool chainSwitch = ui < 0 && usb < 0 && chained >= 0;
        if (ui >= 0 && chained >= 0) chainRequested = false;   // a manual switch wins; retry the chain step
        if (chainSwitch) ui = chained;
        bank.chainBarQuantize.store (chainSwitch);
        sequencer.render (bank, audioSequences, clock, count, sampleRate, midi,
            ui >= 0 ? ui : usb, ui >= 0,
            [this, blockTime] (bool on, int track, int note, int velocity, int sample, bool mirror, bool usbOut)
            {
                if (mirror)
                {
                    const uint8 message[] { uint8 ((on ? 144 : 128) + track), uint8 (note), uint8 (velocity) };
                    generated.addEvent (message, 3, sample);
                }
                if (usbOut) hardware.scheduleNote (on, track, note, velocity, blockTime + sample * 1000.0 / sampleRate);
            },
            [this] (int slot, int sample, bool send)
            {
                if (send)
                {
                    const uint8 message[] { 192, uint8 (slot) };
                    generated.addEvent (message, 2, sample);
                }
            }, [this] { hardware.cancelScheduledNotes(); },
            [this, clock, increment, blockTime, restartControls] (int slot, int first, int end, double tolerance)
            {
                renderHardwareControls (slot, first, end, clock, increment, blockTime, tolerance,
                                        restartControls && first == 0);
            });
        if (bank.sendTempo.load() && (wrapperType == wrapperType_Standalone || hasHostTempo))
        {
            const int bpm = roundToInt (jlimit (5.0, 300.0, clock.bpm));
            if (bpm != sentTempo)
            {
                hardware.scheduleControl ({ 0x30, 0x40, 0x76 }, bpm - 5, 2, blockTime);
                sentTempo = bpm;
            }
        }
        else sentTempo = -1;
        bank.chainBarQuantize.store (false);
        sendPanelControls (blockTime);
        const int record = bank.recordQuantize.load();
        if (record >= 0 && record != sentRecordQuantize)
            hardware.scheduleControl ({ 0x00, 0x00, 0x1f }, record, 1, blockTime);
        sentRecordQuantize = record;
        midi.clear();
        midi.addEvents (generated, 0, count, 0);
    }
    void processBlockBypassed (AudioBuffer<float>& audio, MidiBuffer& midi) override
    {
        audio.clear();
        midi.clear();
        if (bank.playing.load()) hardware.cancelScheduledNotes();
        sequencer.flush (0, [this, &midi] (bool on, int t, int n, int v, int sample, bool mirror, bool usb)
        {
            if (mirror) midi.addEvent (MidiMessage::noteOff (t + 1, n), sample);
            if (usb) hardware.scheduleNote (on, t, n, v, Time::getMillisecondCounterHiRes());
        });
        sequencer.reset (bank.current.load(), bank.editGeneration.load());
        controlSlot = -1;
        bank.playing.store (false);
    }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 16; }
    int getCurrentProgram() override { return bank.current.load(); }
    void setCurrentProgram (int index) override { if (isPositiveAndBelow (index, 16)) bank.hardwareProgram.store (index); }
    const String getProgramName (int index) override { return "Pattern " + String (index + 1); }
    void changeProgramName (int, const String&) override {}
    bool hasEditor() const override { return true; }
    AudioProcessorEditor* createEditor() override;
    void getStateInformation (MemoryBlock& dest) override
    {
        auto copy = state.copyState();
        { const std::lock_guard<std::mutex> lock (snapshotMutex); copy.setProperty ("soundLabSnapshots", snapshotJson, nullptr); }
        copy.setProperty ("patternBank", JSON::toString (bankData(), true), nullptr);
        if (auto xml = copy.createXml()) copyXmlToBinary (*xml, dest);
    }
    void setStateInformation (const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary (data, size))
            if (xml->hasTagName ("SEQTRAK_EDITOR"))
            {
                auto restored = ValueTree::fromXml (*xml);
                { const std::lock_guard<std::mutex> lock (snapshotMutex); snapshotJson = restored.getProperty ("soundLabSnapshots").toString(); }
                restored.removeProperty ("soundLabSnapshots", nullptr);
                if (restored.hasProperty ("patternBank"))
                    loadBank (JSON::parse (restored.getProperty ("patternBank").toString()), true);
                restored.removeProperty ("patternBank", nullptr);
                state.replaceState (restored);
            }
    }

    var bankData()
    {
        Array<var> slots;
        const std::lock_guard<std::mutex> guard (bank.lock);
        for (const auto& p : bank.slots) slots.add (seqtrak::encodePattern (p));
        return seqtrak::object ({ {"format", "seqtrak-editor-bank"}, {"version", 1}, {"slots", slots},
            {"current", bank.current.load()}, {"quantize", bank.quantize.load()}, {"sounds", bank.soundMode.load()},
            {"mirror", bank.mirror.load()}, {"sendProgram", bank.sendProgram.load()},
            {"follow", bank.follow.load()}, {"tempo", bank.tempo.load()},
            {"hardwareMode", bank.hardwareMode.load()}, {"sendTempo", bank.sendTempo.load()},
            {"hardwareFollow", bank.hardwareFollow.load()}, {"recordQuantize", bank.recordQuantize.load()},
            {"trackModes", trackModesData()}, {"chain", chainData()},
            {"chainOn", bank.chainOn.load()}, {"chainLoop", bank.chainLoop.load()} });
    }

    var chainData() const
    {
        Array<var> chain;
        for (int i = 0; i < seqtrak::Bank::chainSize; ++i)
            chain.add (Array<var> { bank.chainSlots[size_t (i)].load(), bank.chainBars[size_t (i)].load() });
        return chain;
    }

    var trackModesData() const
    {
        Array<var> modes;
        for (const auto& mode : bank.trackModes) modes.add (mode.load());
        return modes;
    }

    bool loadBank (const var& data, bool restore)
    {
        const auto* slots = data["slots"].getArray();
        if (data["format"].toString() != "seqtrak-editor-bank" || !seqtrak::integer (data["version"], 1, 1)
            || slots == nullptr || slots->size() != 16 || !seqtrak::integer (data["current"], 0, 15)) return false;
        auto fresh = std::make_unique<std::array<seqtrak::Pattern, 16>>();
        for (int i = 0; i < 16; ++i)
            if (!seqtrak::decodePattern ((*slots)[i], (*fresh)[size_t (i)], voiceSchema, hardware)) return false;
        const std::lock_guard<std::mutex> guard (bank.lock);
        bank.slots = std::move (*fresh);
        bank.current.store (int (data["current"]));
        bank.requested.store (-1); bank.hardwareProgram.store (-1);
        bank.editGeneration.fetch_add (1);
        bank.sequenceRevision.fetch_add (1);
        bank.restored.fetch_add (1);
        if (restore)
        {
            bank.quantize.store (jlimit (0, 2, int (data["quantize"])));
            bank.soundMode.store (jlimit (0, 2, int (data["sounds"])));
            bank.tempo.store (jlimit (5, 300, int (data["tempo"])));
            bank.hardwareMode.store (bool (data["hardwareMode"]));
            bank.sendTempo.store (bool (data["sendTempo"]));
            bank.hardwareFollow.store (data["hardwareFollow"].isVoid() || bool (data["hardwareFollow"]));
            bank.recordQuantize.store (seqtrak::integer (data["recordQuantize"], 0, 5) ? int (data["recordQuantize"]) : -1);
            const auto* modes = data["trackModes"].getArray();
            for (int t = 0; t < 11; ++t)
                bank.trackModes[size_t (t)].store (modes != nullptr && modes->size() == 11
                    && seqtrak::integer ((*modes)[t], 0, 2) ? int ((*modes)[t]) : 0);
            bank.mirror.store (bool (data["mirror"]));
            bank.sendProgram.store (bool (data["sendProgram"]));
            bank.follow.store (bool (data["follow"]));
            const auto* chain = data["chain"].getArray();
            for (int i = 0; i < seqtrak::Bank::chainSize; ++i)
            {
                const bool valid = chain != nullptr && i < chain->size() && (*chain)[i].isArray()
                                   && (*chain)[i].getArray()->size() == 2
                                   && seqtrak::integer ((*chain)[i][0], -1, 15) && seqtrak::integer ((*chain)[i][1], 1, 64);
                bank.chainSlots[size_t (i)].store (valid ? int ((*chain)[i][0]) : -1);
                bank.chainBars[size_t (i)].store (valid ? int ((*chain)[i][1]) : 1);
            }
            bank.chainOn.store (bool (data["chainOn"]));
            bank.chainLoop.store (data["chainLoop"].isVoid() || bool (data["chainLoop"]));
        }
        return true;
    }

    static AudioProcessorValueTreeState::ParameterLayout parameters()
    {
        AudioProcessorValueTreeState::ParameterLayout result;
        const auto add = [&] (const String& id, const String& label, int initial)
        {
            result.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, label, 0, 127, initial));
        };
        add ("master", "Master Volume", 105);
        for (int i = 0; i < tracks.size(); ++i)
            add ("vol" + String (i), tracks[i] + " Volume", 95 - i * 3);
        for (int i = 0; i < 4; ++i) add ("quick" + String (i), "Parameter Knob " + String (i + 1), 64);
        add ("fx", "FX", 43);
        add ("fxlevel", "FX Level", 77);
        add ("highpass", "High Pass", 25);
        add ("repeat", "Repeater", 0);
        for (int t = 0; t < tracks.size(); ++t)
        {
            result.add (std::make_unique<AudioParameterInt> (ParameterID { "hwPattern" + String (t), 2 },
                "Pattern " + tracks[t], 1, 6, 1));
            result.add (std::make_unique<AudioParameterBool> (ParameterID { "hwMute" + String (t), 2 },
                "Mute " + tracks[t], false));
        }
        return result;
    }

    void renderHardwareControls (int slot, int first, int end, seqtrak::Clock clock,
                                 double increment, double blockTime, double tolerance, bool regrid)
    {
        const bool slotChanged = controlSlot != slot;
        controlSlot = slot;
        const auto& stored = audioSequences[size_t (slot)].hardware;
        const auto start = clock.ppq + first * increment;
        for (int t = 0; t < 11; ++t)
        {
            const auto i = size_t (t);
            auto& control = trackControls[i];
            const bool enabled = seqtrak::usesHardware (bank, t);
            const int hostPattern = int (patternParameters[i]->load()) - 1;
            const bool hostMute = muteParameters[i]->load() >= 0.5f;
            const bool initial = control.hostPattern < 0;
            const bool changedPattern = !initial && hostPattern != control.hostPattern;
            const bool changedMute = !initial && hostMute != control.hostMute;
            control.hostPattern = hostPattern;
            control.hostMute = hostMute;
            if (!enabled)
            {
                control.enabled = false;
                control.pending = false;
                bank.displayedPatterns[i].store (-1);
                bank.displayedMutes[i].store (false);
                continue;
            }
            const bool editedPattern = stored[i].pattern != control.stored.pattern;
            const bool editedMute = stored[i].muted != control.stored.muted;
            const bool edited = editedPattern || editedMute;
            if (slotChanged || !control.enabled || editedPattern) control.desired.pattern = stored[i].pattern;
            if (slotChanged || !control.enabled || editedMute) control.desired.muted = stored[i].muted;
            control.stored = stored[i];
            if (changedPattern || (initial && hostPattern != 0)) control.desired.pattern = hostPattern;
            if (changedMute || (initial && hostMute)) control.desired.muted = hostMute;

            if (slotChanged || !control.enabled || edited || changedPattern || changedMute || initial)
            {
                control.pending = true;
                control.at = slotChanged ? start : seqtrak::Sequencer::quantized (bank, clock, start, tolerance);
            }
            else if (control.pending && regrid)
                control.at = seqtrak::Sequencer::quantized (bank, clock, start, tolerance);

            const bool force = slotChanged || !control.enabled || control.force;
            control.enabled = true;
            control.force = force;
            if (!control.pending) continue;
            const int sample = clock.playing
                ? seqtrak::Sequencer::sampleFor (control.at, clock.ppq, increment, end) : first;
            if (sample >= end) continue;
            const double time = blockTime + jmax (sample, first) * 1000.0 / sampleRate;
            if (force || control.sent.pattern != control.desired.pattern)
                hardware.scheduleControl ({ 0x30, 0x50 + t, 0x0f }, control.desired.pattern, 1, time);
            if (force || control.sent.muted != control.desired.muted)
                hardware.scheduleControl ({ 0x30, 0x50 + t, 0x29 }, control.desired.muted ? 0x7d : 0, 1, time);
            control.sent = control.desired;
            bank.displayedPatterns[i].store (control.sent.pattern);
            bank.displayedMutes[i].store (control.sent.muted);
            control.pending = control.force = false;
        }
    }

    struct TrackControl
    {
        seqtrak::HardwareTrack stored, desired, sent;
        int hostPattern = -1;
        bool hostMute = false, pending = false, enabled = false, force = false;
        double at = 0;
    };
    std::array<TrackControl, 11> trackControls {};
    std::array<std::atomic<float>*, 11> patternParameters {}, muteParameters {};
public:
    // Set by the editor: which track/section the panel controls address, and the MASTER/SINGLE switch.
    std::atomic<int> uiTrack { 0 }, uiSection { 0 };
    std::atomic<bool> fxSingle { false };
    std::array<std::atomic<float>*, 11> volumeParameters {};
    std::array<std::atomic<float>*, 4> quickParameters {};
    std::atomic<float>* fxLevelParameter = nullptr, * highPassParameter = nullptr, * repeatParameter = nullptr;
    std::array<int, 11> sentVolume {};
    std::array<int, 4> sentQuick {};
    int sentFxLevel = -1, sentHighPass = -1, sentRepeat = -1;
    bool panelPrimed = false;

    bool chainWasRunning = false, chainRequested = false;
    int chainIndex = 0, chainBarsLeft = 0;
    int64 chainLastBar = 0;

    // Pattern chain: returns a slot to switch to (bar-quantized), or -1.
    int runChain (const seqtrak::Clock& clock, double blockLength)
    {
        int entries = 0;
        while (entries < seqtrak::Bank::chainSize && isPositiveAndBelow (bank.chainSlots[size_t (entries)].load(), 16)) ++entries;
        if (!bank.chainOn.load() || !clock.playing || entries == 0 || clock.barLength <= 0)
        {
            chainWasRunning = false;
            bank.chainPosition.store (-1);
            return -1;
        }
        const auto slotAt = [&] (int i) { return bank.chainSlots[size_t (i)].load(); };
        const auto barsAt = [&] (int i) { return jlimit (1, 64, bank.chainBars[size_t (i)].load()); };
        // The host's bar origin is the start of the *current* bar and moves every bar,
        // so count bars against a fixed grid aligned to it instead.
        const double origin = clock.barOrigin - std::floor (clock.barOrigin / clock.barLength) * clock.barLength;
        const auto position = [&] (double ppq) { return (ppq - origin) / clock.barLength; };
        if (!chainWasRunning)
        {
            chainWasRunning = true;
            chainIndex = 0;
            chainRequested = false;
            const auto start = position (clock.ppq);
            const auto fraction = start - std::floor (start);
            const bool onBar = fraction < 1.0e-6 || fraction > 1.0 - 1.0e-6;
            chainLastBar = int64 (std::floor (start + 1.0e-9));
            chainBarsLeft = barsAt (0) + (onBar ? 0 : 1);
            bank.chainPosition.store (0);
            return slotAt (0);
        }
        const auto end = position (clock.ppq + blockLength);
        const auto bar = int64 (std::floor (end + 1.0e-9));
        while (chainLastBar < bar)
        {
            ++chainLastBar;
            if (--chainBarsLeft > 0) continue;
            int next = chainIndex + 1;
            if (next >= entries)
            {
                if (!bank.chainLoop.load()) { chainBarsLeft = std::numeric_limits<int>::max() / 2; continue; }
                next = 0;
            }
            chainIndex = next;
            chainBarsLeft = barsAt (next);
            chainRequested = false;
            bank.chainPosition.store (chainIndex);
        }
        // Ask for the next step halfway through the last bar, so it lands exactly on the next bar.
        if (!chainRequested && chainBarsLeft == 1 && end - std::floor (end) >= 0.5)
        {
            chainRequested = true;
            int next = chainIndex + 1;
            if (next >= entries)
            {
                if (!bank.chainLoop.load()) return -1;
                next = 0;
            }
            return slotAt (next);
        }
        return -1;
    }

    // Sends panel knobs/faders to SEQTRAK as Control Change when their value changes.
    void sendPanelControls (double time)
    {
        const auto value = [] (std::atomic<float>* p) { return p != nullptr ? roundToInt (p->load()) : 0; };
        if (!panelPrimed)   // don't blast stored values at load; only send real changes
        {
            for (int t = 0; t < 11; ++t) sentVolume[size_t (t)] = value (volumeParameters[size_t (t)]);
            for (int k = 0; k < 4; ++k) sentQuick[size_t (k)] = value (quickParameters[size_t (k)]);
            sentFxLevel = value (fxLevelParameter); sentHighPass = value (highPassParameter); sentRepeat = value (repeatParameter);
            panelPrimed = true;
            return;
        }
        for (int t = 0; t < 11; ++t)
        {
            const int v = value (volumeParameters[size_t (t)]);
            if (v != sentVolume[size_t (t)]) { sentVolume[size_t (t)] = v; hardware.scheduleCC (t, 7, v, time); }
        }
        const int track = jlimit (0, 10, uiTrack.load()), section = uiSection.load();
        for (int k = 0; k < 4; ++k)
        {
            const int v = value (quickParameters[size_t (k)]);
            if (v == sentQuick[size_t (k)]) continue;
            sentQuick[size_t (k)] = v;
            const int cc = quickControl (section, k, track);
            if (cc < 0) continue;
            if (cc == keyControl)   hardware.scheduleControl ({ 0x30, 0x40, 0x7f }, 0x40 + jmin (11, v * 12 / 128), 1, time); // Key C..B
            else if (cc == scaleControl) hardware.scheduleControl ({ 0x30, 0x40, 0x7e }, jmin (7, v * 8 / 128), 1, time);   // Scale 1-8
            else hardware.scheduleCC (track, cc, cc == 26 ? jmin (2, v * 3 / 128) : v, time);
        }
        const int level = value (fxLevelParameter);
        if (level != sentFxLevel)
        {
            sentFxLevel = level;
            if (fxSingle.load()) hardware.scheduleCC (track, 107, level, time);   // Single Effect parameter 1
            else hardware.scheduleCC (0, 102, level, time);                       // Master Effect 1 parameter 1
        }
        const int hp = value (highPassParameter);
        if (hp != sentHighPass) { sentHighPass = hp; hardware.scheduleCC (0, 105, hp, time); }   // Master Effect 2
        const int rep = value (repeatParameter);
        if (rep != sentRepeat) { sentRepeat = rep; hardware.scheduleCC (0, 106, rep, time); }    // Master Effect 3
    }
    int controlSlot = -1, sentTempo = -1, sentRecordQuantize = -1, controlTiming = -1;
    unsigned controlEpoch = 0, controlGeneration = 0;
    bool controlWasPlaying = false;
    double controlExpected = 0;

    AudioProcessorValueTreeState state;
    seqtrak::Bank bank;
    seqtrak::Hardware hardware;
    const var voiceSchema;
    seqtrak::Sequencer sequencer;
    std::array<seqtrak::Sequence, 16> audioSequences {};
    MidiBuffer generated;
    double sampleRate = 0, standalonePpq = 0;
    unsigned audioGeneration = 0, audioRevision = 0;
    unsigned lastSwitch = 0, hardwareRevision = 0;
    std::array<unsigned, 11> soundRevisions {};
    std::mutex snapshotMutex;
    String snapshotJson;
};

using seqtrak::Step;
using seqtrak::Grid;

class Panel final : public Component, public TooltipClient, private Timer, private ListBoxModel
{
public:
    Panel* self() noexcept { return this; }
    explicit Panel (Processor& p)
        : processor (p), presets (parse (BinaryData::Presets_json, BinaryData::Presets_jsonSize)),
          schema (parse (BinaryData::VoiceSchema_json, BinaryData::VoiceSchema_jsonSize)),
          effects (parse (BinaryData::Effects_json, BinaryData::Effects_jsonSize))
    {
        setLookAndFeel (&look);
        setSize (width, height);
        for (int t = 0; t < 11; ++t)
        {
            const String engine = engineFor (t);
            for (const auto& item : *presets.getArray())
                if (item["engine"].toString() == engine)
                {
                    sounds[size_t (t)] = item;
                    break;
                }
            const auto defs = *schema[t].getArray();
            for (const auto& def : defs)
            {
                values[size_t (t)].push_back (int (def["value"]));
                locked[size_t (t)].push_back (bool (def["defaultLock"]));
            }

        }
        pullPattern();
        makeMain();
        refresh();
        startTimerHz (30);
    }

    ~Panel() override
    {
        stopTimer();
        processor.hardware.stopAudition();
        clearDrawer();
        attachments.clear();
        owned.clear();
        setLookAndFeel (nullptr);
    }

    std::function<void (int)> onScale;

    void paint (Graphics& g) override;
    void mouseDown (const MouseEvent&) override;
    void mouseUp (const MouseEvent&) override;
    void mouseDrag (const MouseEvent&) override;
    String getTooltip() override
    {
        const auto p = getMouseXYRelative();
        if (p.x >= 40 && p.x < 840 && p.y >= 283 && p.y < 405)
            return "Step pads. Click a top pad to toggle; drag to paint many steps; hold or right-click "
                   "(or click the lower pad) to edit note, velocity, probability, timing, gate and flam.";
        if (p.x >= 1160 && p.x < 1360 && p.y >= 30 && p.y < 200)
            return "Display: current sound, pattern and values while you turn a control.";
        return {};
    }
    bool painting = false, paintValue = false;
    int lastPaint = -1;
    int heldPad = -1, flashedPad = -1;
    int64 lastFingerprint = -1;
    int safetyTicks = 0;
    double padFlashUntil = 0;
    unsigned padPress = 0;
    void closeDrawer()
    {
        clearDrawer();
        drawer = -1;
        editedStep = -1;
        for (auto* nav : navigation) nav->setToggleState (false, dontSendNotification);
        repaint();
    }

private:
    template <typename T, typename... Args>
    T& make (Component& parent, Rectangle<int> bounds, Args&&... args)
    {
        auto control = std::make_unique<T> (std::forward<Args> (args)...);
        auto& result = *control;
        parent.addAndMakeVisible (result);
        result.setBounds (bounds);
        (buildingDrawer ? temporary : owned).push_back (std::move (control));
        return result;
    }

    TextButton& button (Component& parent, Rectangle<int> area, const String& name,
                        std::function<void()> action, bool warm = true)
    {
        auto& b = make<TextButton> (parent, area, name);
        b.setTooltip (helpFor (name));
        b.setColour (TextButton::buttonColourId, warm ? recess : inset);
        b.setColour (TextButton::buttonOnColourId, warm ? darkOrange : ink);
        b.setColour (TextButton::textColourOffId, warm ? cream : ink);
        b.setColour (TextButton::textColourOnId, warm ? cream : ink);
        b.onClick = [safe = Component::SafePointer<TextButton> (&b), action = std::move (action)]
        {
            MessageManager::callAsync ([safe, action] { if (safe != nullptr) action(); });
        };
        return b;
    }

    Label& label (Component& parent, Rectangle<int> area, const String& text, Colour colour = cream)
    {
        auto& result = make<Label> (parent, area, String(), text);
        result.setFont (face (13, Font::bold));
        result.setColour (Label::textColourId, colour);
        return result;
    }

    ComboBox& combo (Component& parent, Rectangle<int> area, const StringArray& items, int selected)
    {
        auto& result = make<ComboBox> (parent, area);
        result.addItemList (items, 1);
        result.setSelectedItemIndex (selected, dontSendNotification);
        return result;
    }

    Slider& slider (Component& parent, Rectangle<int> area, const String& name, double lo,
                    double hi, double initial, Slider::SliderStyle style, bool warm)
    {
        auto& result = make<Slider> (parent, area, style, Slider::NoTextBox);
        result.setName (name);
        result.setTooltip (name);
        result.setRange (lo, hi, 1.0);
        result.setValue (initial, dontSendNotification);
        if (warm) result.getProperties().set ("orange", true);
        result.onDragEnd = [this] { readout.clear(); repaint(); };
        return result;
    }

    Slider& hostKnob (Rectangle<int> area, const String& id, const String& name, bool warm,
                      Slider::SliderStyle style = Slider::RotaryHorizontalVerticalDrag)
    {
        auto& result = slider (*this, area, name, 0, 127, 64, style, warm);
        if (style == Slider::LinearVertical) result.getProperties().set ("ladder", true);
        attachments.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (processor.state, id, result));
        result.onValueChange = [this, &result]
        {
            if (result.isMouseButtonDown())
            {
                const auto name = result.getName().toUpperCase();
                const int v = int (result.getValue());
                static const StringArray modes { "MONO", "POLY", "CHORD" };
                static const StringArray keys { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
                String shown = String (v);
                if (name == "MODE" && (section == 2 || section == 3)) shown = modes[jmin (2, v * 3 / 128)];
                else if (name == "KEY" && section == 3) shown = keys[jmin (11, v * 12 / 128)];
                else if (name == "SCALE" && section == 3) shown = "SCALE " + String (jmin (7, v * 8 / 128) + 1);
                readout = name + "  " + shown;
            }
            repaint();
        };
        return result;
    }

    void makeMain();
    void refresh();
    void showDrawer (int, bool animate = true);
    void clearDrawer();
    void presetDrawer();
    void voiceDrawer (bool random);
    void effectDrawer();
    void patternDrawer();
    void connectionDrawer();
    void about();
    void editStep (int step);
    void filterPresets (bool restore = false);
    void timerCallback() override;
    Step& stepAt (int step) { return grid[size_t (track)][size_t (page)][size_t (step)]; }

    int getNumRows() override { return int (matches.size()); }
    void paintListBoxItem (int row, Graphics& g, int w, int h, bool selected) override;
    // Double-click (or Return) on a preset loads it, same as the LOAD button.
    void listBoxItemDoubleClicked (int, const MouseEvent&) override { triggerPresetLoad(); }
    // Remember the highlighted preset per track, so the browser comes back where you left it.
    void selectedRowsChanged (int row) override
    {
        if (restoringPresets || !isPositiveAndBelow (row, int (matches.size()))) return;
        presetViews[size_t (track)].preset = matches[size_t (row)];
    }
    struct PresetView { String query, category; int preset = -1; };
    std::array<PresetView, 11> presetViews;
    bool restoringPresets = false;
    void returnKeyPressed (int) override { triggerPresetLoad(); }
    void triggerPresetLoad()
    {
        if (presetLoad != nullptr && presetLoad->isEnabled()) presetLoad->triggerClick();
    }
    void loadPreset (bool pad);
    void toggleCompare();
    void randomize (int t);
    bool canRandomize (int t) const;
    void syncHardware();
    void setVoice (int t, int i, int value);
    void applyVoice (int t, const std::vector<int>& snapshot);
    void checkpoint (int t);
    void snapshotsDrawer();
    void chainDrawer();
    void printDrawer();
    void printToSeqtrak();
    Label* printStatus = nullptr;
    int printSource = -1, printDest = 0;
    std::array<bool, 11> printTracks { true, true, true, true, true, true, true, true, true, true, true };
    void updateChainStatus();
    Label* chainStatus = nullptr;
    std::array<Label*, 16> chainNumbers {};
    int shownChainPosition = -2;
    Array<var> snapshots() const;
    void saveSnapshots (const Array<var>& items);
    String snapshotStatus;
    void chooseSnapshotFile (bool exporting);
    var currentSnapshot (const String& name) const;
    static String engineFor (int t) { return t < 7 ? "Drum" : t < 9 ? "Synth" : t == 9 ? "DX" : "SAMPLER"; }

    PanelLook look;
    class DrawerSlide final : public Timer
    {
    public:
        void start (Component& c)
        {
            // No slide: place the drawer at its final position immediately.
            c.setTopLeftPosition (841, 0);
            target = nullptr;
        }
        void reset() { stopTimer(); target = nullptr; }
    private:
        void timerCallback() override
        {
            if (target == nullptr) { reset(); return; }
            const auto elapsed = Time::getMillisecondCounterHiRes() - started;
            const auto remaining = jmax (0.0, 1.0 - elapsed / 135.0);
            target->setTopLeftPosition (841 + roundToInt (28.0 * remaining * remaining), 0);
            if (remaining == 0.0) reset();
        }
        Component::SafePointer<Component> target;
        double started = 0;
    } drawerSlide;
    Processor& processor;
    std::unique_ptr<TooltipWindow> tooltip;   // exists only while HELP is on
    std::vector<std::unique_ptr<Component>> owned, temporary;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::array<TextButton*, 12> trackButtons {};
    std::array<TextButton*, 20> sectionButtons {};
    std::array<TextButton*, 6> navigation {};
    std::array<Slider*, 4> quick {};
    std::array<Label*, 4> quickNames {};
    std::array<std::array<bool, 2>, 11> mix {};
    std::array<std::vector<int>, 11> values, captured, beforeCompare;
    std::array<std::vector<std::vector<int>>, 11> undo;
    std::array<unsigned, 11> readEpoch {};
    unsigned hardwareRevision = 0, hardwareSession = 0;

    int editedStep = -1;
    bool musical = true;
    bool fxFollowHardware = true;
    std::array<std::vector<bool>, 11> locked;

    std::array<var, 11> sounds;
    std::array<var, 7> padSounds;
    std::array<int, 7> sampleNotes { 60, 61, 62, 63, 64, 65, 66 };
    std::optional<seqtrak::Pattern> clipboard;
    std::unique_ptr<FileChooser> patternFile;
    String patternStatus;
    std::array<int, 11> lengths {};
    int swing = 50, slotTarget = 0;
    unsigned seenSwitch = 0, seenRestore = 0;
    TextButton* lengthButton = nullptr;
    void pullPattern();
    void publishSequence();
    void choosePatternFile (bool whole, bool exporting);
    void lengthMenu();
    const var presets, schema, effects;
    Grid grid {};
    String readout;
    int track = 0, section = 0, page = 0, head = 0, drawer = -1, pattern = 0, bpm = 124;
    int presetPad = 0, effectChoice = 0, randomAmount = 1, voicePart = 0;
    bool buildingDrawer = false, playing = false, comparing = false;
    Component* drawerPanel = nullptr;
    ListBox* presetList = nullptr;
    TextEditor* searchBox = nullptr;
    ComboBox* categoryBox = nullptr;
    StringArray presetCategories;
    Label* presetStatus = nullptr;
    TextButton* presetLoad = nullptr;
    std::vector<int> matches;
    std::unique_ptr<Component> aboutWindow;
    std::unique_ptr<FileChooser> snapshotFile;
};

void Panel::makeMain()
{
    const StringArray left { "VOL+", "VOL-", "DELETE", "MUTE", "SOLO", "PAGE", "PLAY" };
    for (int i = 0; i < left.size(); ++i)
    {
        auto& b = button (*this, {0, 0, 57, 23}, left[i], [this, i]
        {
            if (i < 2)
            {
                if (auto* p = processor.state.getParameter ("vol" + String (track)))
                {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost (jlimit (0.0f, 1.0f, p->getValue() + (i == 0 ? 0.05f : -0.05f)));
                    p->endChangeGesture();
                }
            }
            else if (i == 2) { grid[size_t (track)] = {}; publishSequence(); }
            else if (i < 5) { mix[size_t (track)][size_t (i - 3)] = !mix[size_t (track)][size_t (i - 3)]; publishSequence(); }
            else if (i == 5) { page = (page + 1) % 8; processor.bank.follow.store (false); if (editedStep >= 0) closeDrawer(); }
            else
            {
                if (processor.wrapperType == AudioProcessor::wrapperType_Standalone)
                    processor.bank.standalonePlay.store (!processor.bank.standalonePlay.load());
            }
            repaint();
        }, false);
        b.setTransform (AffineTransform::rotation (-MathConstants<float>::halfPi).translated (5.0f, float ((i + 1) * 59)));
    }

    for (int i = 0; i < 12; ++i)
    {
        const auto title = i == 0 ? String ("ALL") : tracks[i - 1];
        const int column = i < 8 ? i % 4 : 4 + (i - 8) % 2;
        const int row = i < 8 ? i / 4 : (i - 8) / 2;
        const int x = 83 + column * 133, y = 57 + row * 100;
        hostKnob ({ x, y, 76, 72 }, i == 0 ? "master" : "vol" + String (i - 1), title + " Volume", false);
        trackButtons[size_t (i)] = &button (*this, { x - 5, y + 73, 86, 19 }, title,
            [this, i]
            {
                if (i == 0)
                {
                    section = sections.indexOf ("MIXER");
                    refresh();
                    return;
                }
                if (comparing) toggleCompare();
                track = i - 1;
                if (lengthButton != nullptr) lengthButton->setButtonText ("LENGTH " + String (lengths[size_t (track)]));
                processor.hardware.selectTrack (track);
                fxFollowHardware = true;
                voicePart = 0;
                comparing = false;
                refresh();
                if (drawer >= 0) showDrawer (drawer);
                else if (drawerPanel != nullptr) closeDrawer();
            }, false);
        trackButtons[size_t (i)]->getProperties().set ("italic", true);
    }

    for (int i = 0; i < sections.size(); ++i)
        sectionButtons[size_t (i)] = &button (*this, { 848, 31 + i * 17, 101, 16 }, sections[i],
            [this, i] { section = i; refresh(); });
    for (int i = 0; i < 4; ++i)
    {
        quick[size_t (i)] = &hostKnob ({ 966, 43 + i * 77, 78, 63 }, "quick" + String (i), "Parameter", true);
        quickNames[size_t (i)] = &label (*this, { 951, 104 + i * 77, 108, 18 }, {});
        quickNames[size_t (i)]->setJustificationType (Justification::centred);
    }

    {
        // Effect-type selection has no MIDI CC; choose effects in the TRACK FX drawer.
        auto& fxKnob = hostKnob ({ 1066, 43, 74, 67 }, "fx", "FX", true);
        fxKnob.setAlpha (0.4f);
        fxKnob.setEnabled (false);
        fxKnob.setTooltip ("Effect type: choose it in the TRACK FX drawer");
    }
    auto& mode = button (*this, { 1062, 128, 83, 24 }, "MASTER", [] {});
    mode.setClickingTogglesState (true);
    mode.onClick = [this, &mode]
    {
        mode.setButtonText (mode.getToggleState() ? "SINGLE" : "MASTER");
        processor.fxSingle.store (mode.getToggleState());
    };
    mode.setToggleState (processor.fxSingle.load(), dontSendNotification);
    mode.setButtonText (processor.fxSingle.load() ? "SINGLE" : "MASTER");
    hostKnob ({ 1072, 189, 66, 173 }, "fxlevel", "FX Level", true, Slider::LinearVertical);
    hostKnob ({ 1160, 227, 93, 134 }, "highpass", "High Pass", true, Slider::LinearVertical);
    hostKnob ({ 1261, 227, 94, 134 }, "repeat", "Repeater", true, Slider::LinearVertical);

    for (int i = 0; i < drawers.size(); ++i)
        navigation[size_t (i)] = &button (*this, { 849 + i * 85, 384, 82, 26 }, drawers[i],
            [this, i] { if (drawer == i) closeDrawer(); else showDrawer (i); });
    lengthButton = &button (*this, { 235, 12, 112, 24 }, "LENGTH " + String (lengths[size_t (track)]),
                           [this] { lengthMenu(); }, false);
    button (*this, { 353, 12, 80, 24 }, "FOLLOW", [this]
    {
        processor.bank.follow.store (!processor.bank.follow.load());
        refresh();
    }, false);
    button (*this, { 726, 12, 100, 24 }, "ABOUT", [this] { about(); }, false);
    auto& help = button (*this, { 565, 12, 70, 24 }, "HELP", [] {}, false);
    help.setClickingTogglesState (true);
    help.onClick = [this, &help]
    {
        if (help.getToggleState()) tooltip = std::make_unique<TooltipWindow> (this, 350);
        else tooltip.reset();
    };
    auto& scale = combo (*this, { 645, 12, 74, 24 }, { "75%", "100%", "125%", "150%" }, 1);
    scale.setTooltip ("Window size.");
    scale.onChange = [this, &scale]
    {
        if (onScale) onScale (75 + scale.getSelectedItemIndex() * 25);
    };
    const StringArray right { "USB", "MIDI", "BPM+", "BPM-", "SWING", "PROJECT" };
    for (int i = 0; i < right.size(); ++i)
    {
        auto& b = button (*this, { 0, 0, 64, 23 }, right[i], [this, i]
        {
            if (i < 2) showDrawer (5);
            else if (i == 5) showDrawer (4);
            else if (i == 4)
            {
                PopupMenu menu;
                for (int amount = 50; amount <= 75; ++amount)
                    menu.addItem (amount, String (amount) + "%", true, swing == amount);
                menu.showMenuAsync (PopupMenu::Options().withMousePosition(),
                    [safe = Component::SafePointer<Panel> (self())] (int amount)
                    {
                        if (safe == nullptr || amount == 0) return;
                        safe->swing = amount; safe->publishSequence(); safe->repaint();
                    });
            }
            else
            {
                if (processor.wrapperType == AudioProcessor::wrapperType_Standalone)
                    processor.bank.tempo.store (jlimit (5, 300, processor.bank.tempo.load() + (i == 2 ? 1 : -1)));
                repaint();
            }
        });
        b.setTransform (AffineTransform::rotation (-MathConstants<float>::halfPi).translated (1394.0f - 23.0f, float ((i + 1) * 68)));
    }
}

void Panel::refresh()
{
    for (int i = 0; i < 12; ++i) trackButtons[size_t (i)]->setToggleState (i == track + 1, dontSendNotification);
    for (int i = 0; i < sections.size(); ++i) sectionButtons[size_t (i)]->setToggleState (i == section, dontSendNotification);
    const auto& names = knobLabels[size_t (section)];
    for (int i = 0; i < 4; ++i)
    {
        const auto name = i < names.size() ? names[i].toUpperCase() : String (CharPointer_UTF8 ("—"));
        quickNames[size_t (i)]->setText (name, dontSendNotification);
        quick[size_t (i)]->setName (name);
        quick[size_t (i)]->setTooltip (name);
        const bool connected = i < names.size() && quickControl (section, i, track) >= 0;
        quick[size_t (i)]->setEnabled (connected);
        quick[size_t (i)]->setAlpha (connected ? 1.0f : 0.40f);
        if (i < names.size() && !connected)
            quick[size_t (i)]->setTooltip (name + " - no MIDI control on SEQTRAK; edit it in Deep Edit");
    }
    processor.uiTrack.store (track);
    processor.uiSection.store (section);
    repaint();
}

void Panel::paint (Graphics& g)
{
    const auto fill = [&] (Colour c, Rectangle<int> area) { g.setColour (c); g.fillRect (area); };
    const auto text = [&] (Colour c, const String& s, Rectangle<int> area, float size, int flags = Font::bold)
    {
        g.setColour (c); g.setFont (face (size, flags));
        g.drawFittedText (s, area, Justification::centredLeft, 1);
    };
    g.fillAll (grey);
    fill (inset, { 0, 0, 33, height });
    fill (ink.withAlpha (0.25f), { 33, 0, 1, height });
    fill (ink.withAlpha (0.23f), { 602, 49, 1, 212 });
    fill (inset, { 34, 246, 806, 32 });
    fill (ink, { 34, 278, 806, 142 });
    // 4/4 beat marks on steps 1, 5, 9 and 13, in the gap between the two pad rows.
    g.setColour (Colour (0xff62686a));
    for (int beat = 0; beat < 4; ++beat)
        g.fillRoundedRectangle (float (40 + beat * 200 + 15), 343.0f, 16.0f, 3.0f, 1.0f);
    text (ink, "SEQTRAK", { 49, 6, 141, 32 }, 29, Font::bold | Font::italic);
    text (ink.withAlpha (0.76f), "EDITOR", { 173, 10, 90, 29 }, 16, Font::bold | Font::italic);
    text (ink.withAlpha (0.65f), "DRUMS", { 48, 38, 100, 17 }, 11);
    text (ink.withAlpha (0.65f), "SYNTH / SAMPLER", { 615, 38, 190, 17 }, 11);
    text (ink, tracks[track] + "   /   PAGE " + String (page + 1) + " OF 8", { 49, 249, 320, 25 }, 13);
    text (ink, String (bpm) + " BPM    /    " + (playing ? "PLAYING" : "STOPPED"),
          { 672, 249, 155, 25 }, 13);
    text (ink, "SWING " + String (swing) + "%  /  FOLLOW " + (processor.bank.follow.load() ? "ON" : "OFF"), { 393, 249, 260, 25 }, 11);

    for (int i = 0; i < 12; ++i)
    {
        const int column = i < 8 ? i % 4 : 4 + (i - 8) % 2;
        const int row = i < 8 ? i / 4 : (i - 8) / 2;
        for (int j = 0; j < 3; ++j)
        {
            const bool lit = i == 0 ? false : j == 0 ? i == track + 1
                : seqtrak::usesHardware (processor.bank, i - 1)
                    ? j == 1 && processor.bank.displayedMutes[size_t (i - 1)].load()
                    : mix[size_t (i - 1)][size_t (j - 1)];
            g.setColour (lit ? (j == 0 ? Colours::white : j == 1 ? amber : green) : Colour (0xff939a9e));
            g.fillEllipse (float (166 + column * 133), float (75 + row * 100 + j * 13), 6.0f, 6.0f);
        }
    }

    fill (orange, { 840, 0, 560, height });
    fill (recess, { 846, 28, 105, 345 });
    fill (recess, { 1059, 28, 90, 345 });
    fill (recess, { 1156, 198, 100, 175 });
    fill (recess, { 1259, 198, 98, 175 });
    fill (recess, { 1366, 0, 34, height });
    text (cream, "PARAMETER", { 852, 5, 100, 21 }, 13);
    text (cream, "EDIT", { 975, 5, 80, 21 }, 13);
    text (cream, "FX", { 1093, 5, 50, 21 }, 13);
    text (cream, "FX LEVEL", { 1077, 164, 70, 22 }, 12);
    text (cream, "HIGH PASS", { 1174, 202, 78, 22 }, 12);
    text (cream, "REPEATER", { 1275, 202, 78, 22 }, 12);
    text (cream, processor.hardware.verified ? "USB MIDI" : "DEMO", { 1296, 5, 65, 21 }, 12);
    fill (Colour (0xffb94f29), { 1156, 28, 201, 163 });
    fill (Colour (0xffde723c), { 1161, 33, 191, 153 });

    const auto& sound = sounds[size_t (track)];
    const auto& remote = processor.hardware.tracks[size_t (track)];
    auto number = processor.hardware.verified ? remote.number : int (sound["number"]);
    auto name = processor.hardware.verified ? remote.name : sound["name"].toString();
    if (processor.hardware.verified && name.isEmpty() && number == 0 && !processor.hardware.busy())
    {
        name = sound["name"].toString();
        number = int (sound["number"]);
    }
    if (processor.hardware.verified)
    {
        int matchesByName = 0, candidate = 0;
        for (const auto& item : *presets.getArray())
        {
            if (item["engine"].toString() != engineFor (track)) continue;
            if (name.isEmpty() && number != 0 && int (item["number"]) == number) name = item["name"].toString();
            if (number == 0 && track < 10 && name.isNotEmpty() && item["name"].toString() == name)
            { ++matchesByName; candidate = int (item["number"]); }
        }
        if (matchesByName == 1) number = candidate;
        if (name.isEmpty()) name = number == 0 ? "READ HARDWARE" : "SOUND " + String (number);
    }
    constexpr std::array<unsigned, 11> glyphs {
        0b111101101101111, 0b010110010010111, 0b111001111100111,
        0b111001111001111, 0b101101111001001, 0b111100111001111,
        0b111100111101111, 0b111001001001001, 0b111101111101111,
        0b111101111001111, 0b110101110100100
    };
    const bool hardwareTrack = seqtrak::usesHardware (processor.bank, track);
    int hardwarePattern = processor.bank.displayedPatterns[size_t (track)].load();
    if (hardwareTrack && hardwarePattern < 0)
    {
        const std::lock_guard<std::mutex> guard (processor.bank.lock);
        hardwarePattern = processor.bank.slots[size_t (pattern)].sequence.hardware[size_t (track)].pattern;
    }
    const auto top = hardwareTrack ? "P" + String (hardwarePattern + 1)
                                   : "P" + String (pattern + 1).paddedLeft ('0', 2);
    const auto bottom = readout.isEmpty() ? (number == 0 ? String() : String (number)) : readout.fromLastOccurrenceOf (" ", false, false);
    for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 20; ++x)
        {
            const auto& row = y < 6 ? top : bottom;
            const auto column = x - (21 - 4 * row.length()) / 2;
            const auto digit = column / 4;
            const auto glyphRow = y % 6;
            const auto glyphColumn = column % 4;
            bool active = false;
            if (column >= 0 && digit < row.length() && glyphColumn < 3 && glyphRow < 5)
            {
                const int glyph = row[digit] == 'P' ? 10 : int (row[digit]) - '0';
                if (isPositiveAndBelow (glyph, int (glyphs.size())))
                    active = (glyphs[size_t (glyph)] & (1u << (14 - glyphRow * 3 - glyphColumn))) != 0;
            }
            g.setColour (active ? darkOrange : Colour (0xffc65d30));
            g.fillEllipse (float (1171 + x * 9), float (42 + y * 8), 5.0f, 5.0f);
        }
    text (cream, readout.isEmpty() ? name.toUpperCase() : readout, { 1171, 143, 171, 21 }, 14);
    text (cream, (number == 0 ? String ("--") : "#" + String (number)) + "  /  PATTERN " + String (pattern + 1) + "  /  " + tracks[track],
          { 1171, 164, 171, 17 }, 10);

    if (flashedPad >= 0 && Time::getMillisecondCounterHiRes() < padFlashUntil)
        for (int pad = 0; pad < 7; ++pad)
        {
            g.setColour (pad == flashedPad ? cream : darkOrange);
            g.fillEllipse (float (1174 + pad * 24), 186.0f, 8.0f, 8.0f);
        }

    for (int row = 0; row < 2; ++row)
        for (int i = 0; i < 16; ++i)
        {
            const auto x = 40 + i * 50, y = 283 + row * 66;
            const auto& step = stepAt (i);
            fill (row == 0 ? grey : inset, { x, y + 6, 46, 46 });
            g.setColour (Colour (0xff62686a));
            g.fillRoundedRectangle (float (x + 9), float (y + 25), 28.0f, 4.0f, 2.0f);
            if ((!hardwareTrack && step.on) || (playing && page * 16 + i == head))
            {
                const auto lit = playing && page * 16 + i == head ? Colours::white : track < 7 ? green : purple;
                g.setColour (lit.withAlpha (0.23f));
                g.fillRoundedRectangle (float (x + 7), float (y + 23), 32.0f, 8.0f, 3.0f);
                g.setColour (lit.withAlpha (row == 0 || (playing && page * 16 + i == head) ? 1.0f : 0.65f));
                g.fillRoundedRectangle (float (x + 10), float (y + 25), 26.0f, 4.0f, 2.0f);
            }
            if (row == 0 || editedStep == i)
                text (ink, row == 0 ? String (i + 1).paddedLeft ('0', 2) : String (step.velocity),
                  { x + 7, y + 39, 36, 17 }, 11);
        }
}



void Panel::mouseDown (const MouseEvent& event)
{
    const auto x = event.x - 40;
    if (x < 0 || x >= 800 || x % 50 >= 46) return;
    if (!((event.y >= 289 && event.y < 335) || (event.y >= 355 && event.y < 401))) return;
    const auto step = x / 50;
    if (event.y >= 349 || event.mods.isPopupMenu()) editStep (step);
    else if (event.y < 335)
    {
        heldPad = step;
        // A drag from here paints this value across every pad the mouse passes over.
        painting = false;
        paintValue = !stepAt (step).on;
        lastPaint = step;
        Timer::callAfterDelay (400, [safe = Component::SafePointer<Panel> (self()), step, press = ++padPress]
        {
            if (safe == nullptr || safe->heldPad != step || safe->padPress != press || !safe->isMouseButtonDown()) return;
            safe->heldPad = -1;
            safe->editStep (step);
        });
    }
}

void Panel::mouseDrag (const MouseEvent& event)
{
    if (heldPad < 0 && !painting) return;
    const auto x = event.x - 40;
    if (x < 0 || x >= 800 || event.y < 280 || event.y >= 410) return;
    const int step = jlimit (0, 15, x / 50);
    if (step == lastPaint && painting) return;
    if (!painting)
    {
        if (step == heldPad) return;             // still on the first pad: keep hold-to-edit
        painting = true;
        stepAt (heldPad).on = paintValue;        // the pad where the drag started
        heldPad = -1;                            // cancels hold-to-edit
    }
    for (int i = jmin (lastPaint, step); i <= jmax (lastPaint, step); ++i)   // fill skipped pads on fast drags
        stepAt (i).on = paintValue;
    lastPaint = step;
    publishSequence();
    repaint (33, 246, 807, 174);
}

void Panel::mouseUp (const MouseEvent&)
{
    if (painting) { painting = false; lastPaint = -1; return; }
    if (heldPad < 0) return;
    auto& step = stepAt (heldPad);
    heldPad = -1;
    step.on = !step.on;
    publishSequence();
    repaint();
}

void Panel::clearDrawer()
{
    processor.hardware.stopAudition();
    drawerSlide.reset();
    if (presetList != nullptr) presetList->setModel (nullptr);
    presetList = nullptr;
    searchBox = nullptr;
    categoryBox = nullptr;
    presetStatus = nullptr;
    presetLoad = nullptr;
    chainStatus = nullptr;
    printStatus = nullptr;
    chainNumbers.fill (nullptr);
    shownChainPosition = -2;
    while (!temporary.empty()) temporary.pop_back();
    drawerPanel = nullptr;
}

void Panel::showDrawer (int index, bool animate)
{
    clearDrawer();
    drawer = index;
    editedStep = -1;
    buildingDrawer = true;
    drawerPanel = &make<Component> (*this, { 841, 0, 525, 378 });
    auto& panel = *drawerPanel;
    auto& bg = make<Label> (panel, panel.getLocalBounds());
    bg.setColour (Label::backgroundColourId, orange);
    bg.setInterceptsMouseClicks (false, false);
    label (panel, { 15, 10, 205, 30 }, index == 8 ? String ("PRINT TO SEQTRAK") : index == 7 ? String ("PATTERN CHAIN")
                                       : (index == 6 ? String ("SNAPSHOTS") : drawers[index]) + "  /  " + tracks[track]).setFont (face (19, Font::bold));
    label (panel, { 313, 13, 128, 24 }, processor.hardware.verified ? "USB MIDI" : "DEMO", darkOrange);
    button (panel, { 462, 12, 49, 24 }, "CLOSE", [this] { closeDrawer(); });

    switch (index)
    {
        case 0: presetDrawer(); break;
        case 1: voiceDrawer (false); break;
        case 2: effectDrawer(); break;
        case 3: voiceDrawer (true); break;
        case 4: patternDrawer(); break;
        case 5: connectionDrawer(); break;
        case 6: snapshotsDrawer(); break;
        case 7: chainDrawer(); break;
        case 8: printDrawer(); break;
        default: break;
    }
    buildingDrawer = false;
    for (int i = 0; i < drawers.size(); ++i) navigation[size_t (i)]->setToggleState (i == index, dontSendNotification);
    if (animate) drawerSlide.start (*drawerPanel);
}

void Panel::presetDrawer()
{
    auto& panel = *drawerPanel;
    searchBox = &make<TextEditor> (panel, { 16, 49, 265, 28 });
    searchBox->setTextToShowWhenEmpty ("Search by name or number", ink.withAlpha (0.65f));
    const auto engine = engineFor (track);
    presetCategories = { "All categories" };
    for (const auto& item : *presets.getArray())
        if (item["engine"].toString() == engine)
            presetCategories.addIfNotAlreadyThere (item["category"].toString());
    const auto& view = presetViews[size_t (track)];
    categoryBox = &combo (panel, { 293, 49, 216, 28 }, presetCategories, jmax (0, presetCategories.indexOf (view.category)));
    categoryBox->onChange = [this]
    {
        presetViews[size_t (track)].category = categoryBox->getText();
        filterPresets();
    };
    searchBox->setText (view.query, false);
    searchBox->onTextChange = [this]
    {
        presetViews[size_t (track)].query = searchBox->getText();
        filterPresets();
    };
    presetList = &make<ListBox> (panel, { 16, 88, 493, 225 }, "Factory presets", static_cast<ListBoxModel*> (this));
    presetList->setRowHeight (25);
    filterPresets (true);
    presetStatus = &label (panel, { 16, 317, 489, 19 }, processor.hardware.verified ? processor.hardware.status
        : "2,032 DRUM / SYNTH / DX SOUNDS + 392 FACTORY SAMPLES", darkOrange);
    if (track == 10)
    {
        StringArray pads;
        for (int i = 1; i <= 7; ++i) pads.add ("Pad " + String (i));
        auto& select = combo (panel, { 16, 342, 125, 26 }, pads, presetPad);
        select.onChange = [this, &select]
        {
            presetPad = jmax (0, select.getSelectedItemIndex());
            MessageManager::callAsync ([safe = Component::SafePointer<Panel> (self())]
                { if (safe != nullptr && safe->drawer == 0) safe->showDrawer (0); });
        };
        label (panel, { 148, 342, 83, 26 }, "MIDI NOTE", darkOrange);
        auto& note = slider (panel, { 228, 342, 62, 26 }, "Audition MIDI note", 0, 127,
                             sampleNotes[size_t (presetPad)], Slider::LinearHorizontal, false);
        note.setTextBoxStyle (Slider::TextBoxRight, false, 47, 24);
        note.onValueChange = [this, &note] { sampleNotes[size_t (presetPad)] = int (note.getValue()); };
        presetLoad = &button (panel, { 365, 342, 144, 26 }, "LOAD ON PAD", [this] { loadPreset (true); });
    }
    else presetLoad = &button (panel, { 365, 342, 144, 26 }, "LOAD ON TRACK", [this] { loadPreset (false); });
    presetLoad->setEnabled (!processor.hardware.busy());
}

void Panel::filterPresets (bool restore)
{
    const auto query = searchBox->getText().trim();
    const auto category = categoryBox->getSelectedItemIndex();
    const auto engine = engineFor (track);
    const auto& items = *presets.getArray();
    matches.clear();
    for (int i = 0; i < items.size(); ++i)
    {
        const auto& p = items.getReference (i);
        if (p["engine"].toString() == engine
            && (category <= 0 || p["category"].toString() == presetCategories[category])
            && (p["name"].toString().containsIgnoreCase (query) || p["number"].toString().contains (query)))
            matches.push_back (i);
    }
    presetList->updateContent();
    restoringPresets = true;
    presetList->deselectAllRows();
    int row = 0;
    if (restore)
    {
        const auto it = std::find (matches.begin(), matches.end(), presetViews[size_t (track)].preset);
        if (it != matches.end()) row = int (it - matches.begin());
    }
    if (!matches.empty())
    {
        presetList->selectRow (row);
        presetList->scrollToEnsureRowIsOnscreen (jmin (int (matches.size()) - 1, row + 4));
        presetList->scrollToEnsureRowIsOnscreen (row);
    }
    restoringPresets = false;
    if (!matches.empty() && !restore) presetViews[size_t (track)].preset = matches[size_t (row)];
    presetList->repaint();
}

void Panel::paintListBoxItem (int row, Graphics& g, int w, int h, bool selected)
{
    if (!isPositiveAndBelow (row, int (matches.size()))) return;
    const auto item = presets[matches[size_t (row)]];
    g.fillAll (selected ? darkOrange : row % 2 == 0 ? recess : orange);
    g.setColour (selected ? cream : darkOrange);
    g.setFont (face (14, selected ? Font::bold : Font::plain));
    g.drawText ("#" + item["number"].toString(), 9, 0, 55, h, Justification::centredLeft);
    g.drawText (item["name"].toString(), 69, 0, w - 204, h, Justification::centredLeft, true);
    g.drawText (item["category"].toString(), w - 131, 0, 122, h, Justification::centredRight, true);
}

void Panel::loadPreset (bool pad)
{
    const auto row = presetList->getSelectedRow();
    if (!isPositiveAndBelow (row, int (matches.size()))) return;
    const auto item = presets[matches[size_t (row)]];
    if (processor.hardware.verified)
    {
        processor.hardware.read (track, int (item["number"]), presetPad);
        return;
    }
    sounds[size_t (track)] = item;
    if (pad) padSounds[size_t (presetPad)] = item;
    {
        const std::lock_guard<std::mutex> guard (processor.bank.lock);
        auto& active = processor.bank.slots[size_t (pattern)];
        if (pad) active.samples[size_t (presetPad)] = int (item["number"]);
        else
        {
            auto& sound = active.sounds[size_t (track)];
            sound = {};
            sound.number = int (item["number"]);
            sound.name = item["name"].toString();
        }
    }
    closeDrawer();
}

void Panel::voiceDrawer (bool random)
{
    auto& panel = *drawerPanel;
    const auto t = size_t (track);
    const auto defs = *schema[track].getArray();
    if (!random)
    {
        auto& hw = processor.hardware;
        button (panel, { 219, 48, 131, 27 }, "READ HARDWARE", [this] { processor.hardware.read (track); })
            .setEnabled (hw.verified && !hw.busy());
        auto& audition = button (panel, { 361, 48, 148, 27 }, "HOLD TO AUDITION", [] {});
        audition.onStateChange = [this, &audition]
        {
            if (audition.isDown())
            {
                if (!processor.hardware.busy())
                    processor.hardware.audition (track, track == 10 ? sampleNotes[size_t (presetPad)] : 60);
            }
            else processor.hardware.stopAudition();
        };
        audition.setEnabled (hw.verified && !hw.busy());
        button (panel, { 405, 12, 52, 24 }, "SNAPS", [this] { showDrawer (6); });
    }
    auto& scroller = make<Viewport> (panel, { 16, random ? 118 : 86, 493, random ? 250 : 282 });
    auto& rows = make<Component> (panel, { 0, 0, 475, 0 });
    scroller.setViewedComponent (&rows, false);
    scroller.setScrollBarsShown (true, false);
    int y = 0;
    if (random)
    {
        auto& amount = combo (panel, { 16, 47, 94, 26 }, { "Subtle", "Medium", "Wild" }, randomAmount);
        amount.onChange = [this, &amount] { randomAmount = jmax (0, amount.getSelectedItemIndex()); };
        button (panel, { 117, 47, 95, 26 }, "RANDOMIZE", [this] { randomize (track); showDrawer (3); })
            .setEnabled (canRandomize (track));
        button (panel, { 219, 47, 93, 26 }, "ALL THREE", [this]
        {
            if (canRandomize (7) && canRandomize (8) && canRandomize (9))
                for (int i = 7; i <= 9; ++i) randomize (i);
            showDrawer (3);
        }).setEnabled (canRandomize (7) && canRandomize (8) && canRandomize (9));
        auto& mode = combo (panel, { 319, 47, 113, 26 }, { "Musical DX", "Experimental DX" }, musical ? 0 : 1);
        mode.onChange = [this, &mode] { musical = mode.getSelectedItemIndex() == 0; };
        button (panel, { 439, 47, 70, 26 }, "UNDO", [this, t]
        {
            if (processor.hardware.busy() || undo[t].empty()) return;
            applyVoice (int (t), undo[t].back());
            undo[t].pop_back();
            comparing = false;
            showDrawer (3);
        }).setEnabled (!undo[t].empty() && !processor.hardware.busy());
        bool complete = !processor.hardware.busy();
        for (const auto& def : defs) complete &= processor.hardware.canEdit (track, def);
        button (panel, { 16, 80, 100, 26 }, "CAPTURE A", [this, t]
        {
            if (comparing) toggleCompare();
            captured[t] = values[t];
            showDrawer (3);
        }).setEnabled (complete);
        button (panel, { 123, 80, 110, 26 }, comparing ? "RETURN TO B" : "COMPARE A/B",
                [this] { toggleCompare(); showDrawer (3); }).setEnabled (complete && !captured[t].empty());
        label (panel, { 242, 80, 267, 26 }, complete ? (comparing ? "A / CAPTURED" : "B / CURRENT")
                                                       : "READ HARDWARE TO UNLOCK EDITING", darkOrange);
        StringArray groups;
        for (const auto& def : defs) groups.addIfNotAlreadyThere (def["group"].toString());
        for (int i = 0; i < groups.size(); ++i)
        {
            const auto group = groups[i];
            bool allLocked = true;
            for (int k = 0; k < defs.size(); ++k)
                if (defs[k]["group"].toString() == group) allLocked &= locked[t][size_t (k)];
            auto& lock = button (rows, { (i % 4) * 117, (i / 4) * 27, 112, 24 }, group.toUpperCase(),
                [this, t, group, allLocked]
                {
                    const auto& definitions = *schema[int (t)].getArray();
                    for (int k = 0; k < definitions.size(); ++k)
                        if (definitions[k]["group"].toString() == group) locked[t][size_t (k)] = !allLocked;
                    showDrawer (3);
                });
            lock.setToggleState (allLocked, dontSendNotification);
        }
        y = ((groups.size() + 3) / 4) * 27 + 5;
    }
    else if (track >= 7 && track <= 9)
    {
        StringArray units;
        if (track == 9) units.add ("DX Common");
        for (int i = 1; i <= (track == 9 ? 4 : 8); ++i)
            units.add ((track == 9 ? "Operator " : "Element ") + String (i));
        auto& unit = combo (panel, { 16, 48, 180, 27 }, units, voicePart);
        unit.onChange = [this, &unit]
        {
            voicePart = unit.getSelectedItemIndex();
            MessageManager::callAsync ([safe = Component::SafePointer<Panel> (self())]
                { if (safe != nullptr && safe->drawer == 1) safe->showDrawer (1); });
        };
    }
    else label (panel, { 16, 46, 250, 28 }, "TRACK SOUND", darkOrange);

    for (int i = 0; i < defs.size(); ++i)
    {
        const auto def = defs.getReference (i);
        const auto id = def["id"].toString();
        if (!random && track >= 7 && track <= 9)
        {
            const auto prefix = track == 9 ? "o" + String (voicePart - 1) + "." : "e" + String (voicePart) + ".";
            if (track == 9 && voicePart == 0 ? id.startsWithChar ('o') && id.containsChar ('.') : !id.startsWith (prefix))
                continue;
        }
        auto title = def["label"].toString();
        if (random && id.containsChar ('.')) title = id.upToFirstOccurrenceOf (".", false, false).toUpperCase() + "  " + title;
        const bool editable = processor.hardware.canEdit (track, def);
        if (!editable && !processor.hardware.busy()) title += " / unread";
        label (rows, { 0, y, 219, 27 }, title, darkOrange);
        const auto k = size_t (i);
        if (auto* options = def["options"].getArray())
        {
            StringArray list;
            for (const auto& option : *options) list.add (option.toString());
            auto& choice = combo (rows, { 220, y + 1, 184, 25 }, list, values[t][k] - int (def["min"]));
            choice.setEnabled (editable);
            choice.onChange = [this, t, k, low = int (def["min"]), &choice]
            {
                checkpoint (int (t));
                setVoice (int (t), int (k), low + choice.getSelectedItemIndex());
            };
        }
        else
        {
            auto& fader = slider (rows, { 220, y, 184, 26 }, title, int (def["min"]), int (def["max"]),
                                   values[t][k], Slider::LinearHorizontal, false);
            fader.setTextBoxStyle (Slider::TextBoxRight, false, 47, 22);
            fader.setEnabled (editable);
            // Double-click resets the parameter to its default value (undoable).
            fader.setDoubleClickReturnValue (true, jlimit (int (def["min"]), int (def["max"]),
                                                           int (def.getProperty ("value", def["min"]))));
            fader.onDragStart = [this, t] { checkpoint (int (t)); };
            fader.onValueChange = [this, t, k, &fader]
            {
                if (!fader.isMouseButtonDown()) checkpoint (int (t));
                setVoice (int (t), int (k), int (fader.getValue()));
            };
        }
        auto& lock = button (rows, { 413, y + 1, 57, 25 }, "LOCK",
                             [this, t, k] { locked[t][k] = !locked[t][k]; });
        lock.setClickingTogglesState (true);
        lock.setToggleState (locked[t][k], dontSendNotification);
        y += 29;
    }
    rows.setSize (475, y);
}

void Panel::checkpoint (int t)
{
    auto& history = undo[size_t (t)];
    history.push_back (values[size_t (t)]);
    if (history.size() > 40) history.erase (history.begin());
    comparing = false;
}

void Panel::setVoice (int t, int i, int value)
{
    const auto& defs = *schema[t].getArray();
    const auto& def = defs[i];
    auto& hw = processor.hardware;
    if (!hw.canEdit (t, def)) return;
    const auto id = def["id"].toString();
    auto& current = values[size_t (t)];
    int max = int (def["max"]);
    int wave = -1;
    if (id.endsWith (".wave") || id.endsWith (".bank"))
    {
        const auto prefix = id.upToLastOccurrenceOf (".", true, false);
        for (int k = 0; k < defs.size(); ++k)
        {
            if (id.endsWith (".wave") && defs[k]["id"].toString() == prefix + "bank" && current[size_t (k)] == 1)
                max = 2048;
            if (id.endsWith (".bank") && defs[k]["id"].toString() == prefix + "wave") wave = k;
        }
    }
    current[size_t (i)] = jlimit (int (def["min"]), max, value);
    hw.setVoice (t, def, current[size_t (i)]);
    {
        const std::lock_guard<std::mutex> guard (processor.bank.lock);
        processor.bank.slots[size_t (pattern)].sounds[size_t (t)].voice[seqtrak::address (def)] = current[size_t (i)];
    }
    if (wave >= 0 && value == 1 && current[size_t (wave)] > 2048) setVoice (t, wave, 2048);
}

void Panel::applyVoice (int t, const std::vector<int>& snapshot)
{
    const auto& defs = *schema[t].getArray();
    for (const bool banks : { true, false })
        for (int i = 0; i < defs.size(); ++i)
            if (defs[i]["id"].toString().endsWith (".bank") == banks && values[size_t (t)][size_t (i)] != snapshot[size_t (i)])
                setVoice (t, i, snapshot[size_t (i)]);
}

void Panel::toggleCompare()
{
    const auto t = size_t (track);
    if (captured[t].empty() || processor.hardware.busy()) return;
    if (comparing) applyVoice (track, beforeCompare[t]);
    else
    {
        beforeCompare[t] = values[t];
        applyVoice (track, captured[t]);
    }
    comparing = !comparing;
}

bool Panel::canRandomize (int t) const
{
    if (processor.hardware.busy()) return false;
    const auto& defs = *schema[t].getArray();
    const auto prefix = "e" + String (voicePart) + ".";
    for (int i = 0; i < defs.size(); ++i)
        if (!locked[size_t (t)][size_t (i)]
            && (t < 7 || t >= 9 || defs[i]["id"].toString().startsWith (prefix))
            && !processor.hardware.canEdit (t, defs[i])) return false;
    return true;
}

void Panel::randomize (int t)
{
    if (!canRandomize (t)) return;
    if (comparing) toggleCompare();
    checkpoint (t);
    const auto& defs = *schema[t].getArray();
    const auto prefix = "e" + String (voicePart) + ".";
    const double amount = std::array<double, 3> { 0.12, 0.4, 1.0 }[size_t (randomAmount)];
    auto& rng = Random::getSystemRandom();
    for (int i = 0; i < defs.size(); ++i)
    {
        const auto& def = defs[i];
        const auto id = def["id"].toString();
        if (locked[size_t (t)][size_t (i)] || (t >= 7 && t < 9 && !id.startsWith (prefix))) continue;
        const auto category = def["category"].toString();
        const bool options = def["options"].isArray();
        const int lo = int (def["min"]), hi = int (def["max"]);
        const int current = values[size_t (t)][size_t (i)];
        int target = lo + rng.nextInt (hi - lo + 1);
        if (category == "filter" && id.endsWith ("resonance")) target = rng.nextInt (81);
        if (category == "feedback" && !options) target = rng.nextInt (75);
        if (category == "level") target = 35 + rng.nextInt (71);
        if (category == "lfo" && !options) target = rng.nextInt (id.endsWith ("Speed") ? hi + 1 : 40);
        if (musical && t == 9)
        {
            if (id.endsWith (".env3") || (category == "freq" && (id.endsWith ("mode") || id.endsWith ("fine"))))
            {
                setVoice (t, i, 0);
                continue;
            }
            if (category == "freq" && id.endsWith ("coarse"))
                target = std::array<int, 11> { 0, 1, 1, 1, 2, 2, 3, 4, 5, 6, 8 }[size_t (rng.nextInt (11))];
            if (category == "freq" && id.endsWith ("detune")) target = 61 + rng.nextInt (7);
        }
        setVoice (t, i, options ? (rng.nextDouble() < amount ? target : current)
                                : roundToInt (current + (target - current) * amount));
    }
}

void Panel::effectDrawer()
{
    auto& panel = *drawerPanel;
    StringArray names;
    for (const auto& effect : *effects.getArray()) names.add (effect["name"].toString());
    auto& hw = processor.hardware;
    const auto& remote = hw.tracks[size_t (track)];
    seqtrak::Sound local;
    { const std::lock_guard<std::mutex> guard (processor.bank.lock);
      local = processor.bank.slots[size_t (pattern)].sounds[size_t (track)]; }
    if (fxFollowHardware)
    {
        effectChoice = -1;
        for (int i = 0; i < effects.getArray()->size(); ++i)
            if (int (effects[i]["value"]) == (hw.verified ? remote.effect : local.effect)) effectChoice = i;
    }
    auto& choice = combo (panel, { 16, 49, 243, 27 }, names, effectChoice);
    choice.setTextWhenNothingSelected ("Choose an effect");
    choice.onChange = [this, &choice]
    {
        fxFollowHardware = false;
        effectChoice = choice.getSelectedItemIndex();
        MessageManager::callAsync ([safe = Component::SafePointer<Panel> (self())]
            { if (safe != nullptr && safe->drawer == 2) safe->showDrawer (2); });
    };
    button (panel, { 269, 49, 107, 27 }, "READ FX", [this]
        { fxFollowHardware = true; processor.hardware.readFx (track); }).setEnabled (hw.verified && !hw.busy());
    button (panel, { 385, 49, 124, 27 }, "APPLY EFFECT", [this]
    {
        if (effectChoice < 0) return;
        fxFollowHardware = true;
        const int type = int (effects[effectChoice]["value"]);
        if (processor.hardware.verified) processor.hardware.applyFx (track, type);
        else
        {
            { const std::lock_guard<std::mutex> guard (processor.bank.lock);
              auto& sound = processor.bank.slots[size_t (pattern)].sounds[size_t (track)];
              sound.effect = type; sound.fx.clear(); }
            showDrawer (2, false);
        }
    }).setEnabled (!hw.busy() && effectChoice >= 0);
    label (panel, { 16, 80, 490, 21 }, hw.verified ? hw.status : "FX CONTROLS / DEMO", darkOrange);
    if (effectChoice < 0) return;
    const int type = int (effects[effectChoice]["value"]);
    auto& scroller = make<Viewport> (panel, { 16, 107, 493, 262 });
    auto& rows = make<Component> (panel, { 0, 0, 475, 0 });
    scroller.setViewedComponent (&rows, false);
    scroller.setScrollBarsShown (true, false);
    int y = 0;
    for (const auto& p : *effects[effectChoice]["params"].getArray())
    {
        const auto name = p["label"].toString();
        label (rows, { 0, y, 209, 27 }, name, darkOrange);
        const int lo = int (p["min"]), hi = int (p["max"]);
        const int offset = int (p["offset"]);
        const auto& fxValues = hw.verified ? remote.fx : local.fx;
        const auto found = fxValues.find (offset);
        const bool known = (hw.verified ? remote.effect : local.effect) == type && found != fxValues.end();
        auto& fader = slider (rows, { 213, y, 256, 27 }, name, lo, hi,
                               known ? found->second : hw.verified ? lo : (lo + hi) / 2,
                               Slider::LinearHorizontal, false);
        fader.setEnabled (!hw.verified || (!hw.busy() && known));
        const auto commit = [this, type, offset, &fader]
        {
            const int value = int (fader.getValue());
            if (processor.hardware.verified) processor.hardware.setFx (track, type, offset, value);
            else
            {
                const std::lock_guard<std::mutex> guard (processor.bank.lock);
                auto& sound = processor.bank.slots[size_t (pattern)].sounds[size_t (track)];
                if (sound.effect != type) { sound.effect = type; sound.fx.clear(); }
                sound.fx[offset] = value;
            }
        };
        fader.onDragEnd = commit;
        fader.onValueChange = [&fader, commit] { if (!fader.isMouseButtonDown()) commit(); };
        fader.setTooltip (p["range"].toString());
        fader.setTextBoxStyle (Slider::TextBoxRight, false, 46, 22);
        y += 30;
    }
    rows.setSize (475, y);
}

void Panel::pullPattern()
{
    const auto& bank = processor.bank;
    const std::lock_guard<std::mutex> guard (processor.bank.lock);
    pattern = bank.current.load();
    const auto& p = bank.slots[size_t (pattern)];
    grid = p.sequence.grid;
    mix = p.sequence.mix;
    lengths = p.sequence.lengths;
    swing = p.sequence.swing;
    for (int t = 0; t < 11; ++t)
    {
        const auto& sound = p.sounds[size_t (t)];
        const auto& defs = *schema[t].getArray();
        for (int i = 0; i < defs.size(); ++i)
        {
            const auto found = sound.voice.find (seqtrak::address (defs[i]));
            values[size_t (t)][size_t (i)] = found != sound.voice.end() ? found->second : int (defs[i]["value"]);
        }
        if (sound.number != 0 || sound.name.isNotEmpty())
        {
            auto name = sound.name;
            if (name.isEmpty())
                for (const auto& item : *presets.getArray())
                    if (item["engine"].toString() == engineFor (t) && int (item["number"]) == sound.number)
                    { name = item["name"].toString(); break; }
            sounds[size_t (t)] = seqtrak::object ({ {"name", name}, {"number", sound.number} });
        }
        else
        {
            sounds[size_t (t)] = var();
            for (const auto& item : *presets.getArray())
                if (item["engine"].toString() == engineFor (t)) { sounds[size_t (t)] = item; break; }
        }
    }
    if (lengthButton != nullptr) lengthButton->setButtonText ("LENGTH " + String (lengths[size_t (track)]));
}

void Panel::publishSequence()
{
    const std::lock_guard<std::mutex> guard (processor.bank.lock);
    auto& sequence = processor.bank.slots[size_t (pattern)].sequence;
    sequence.grid = grid;
    sequence.lengths = lengths;
    sequence.mix = mix;
    sequence.swing = swing;
    processor.bank.sequenceRevision.fetch_add (1);
}

void Panel::lengthMenu()
{
    PopupMenu menu;
    for (int pageIndex = 0; pageIndex < 8; ++pageIndex)
    {
        PopupMenu part;
        for (int n = pageIndex * 16 + 1; n <= (pageIndex + 1) * 16; ++n)
            part.addItem (n, String (n), true, n == lengths[size_t (track)]);
        menu.addSubMenu (String (pageIndex * 16 + 1) + "-" + String ((pageIndex + 1) * 16), part);
    }
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (lengthButton),
        [safe = Component::SafePointer<Panel> (self()), t = track] (int result)
        {
            if (safe == nullptr || result == 0) return;
            safe->lengths[size_t (t)] = result;
            safe->publishSequence();
            safe->lengthButton->setButtonText ("LENGTH " + String (safe->lengths[size_t (safe->track)]));
            safe->repaint();
        });
}

void Panel::patternDrawer()
{
    auto& panel = *drawerPanel;
    auto& bank = processor.bank;
    StringArray names;
    {
        const std::lock_guard<std::mutex> guard (bank.lock);
        for (const auto& slot : bank.slots) names.add (slot.name);
    }
    label (panel, { 16, 45, 493, 23 }, "ACTIVE SLOT " + String (pattern + 1) + "  /  " + names[pattern], darkOrange);
    auto& chainButton = button (panel, { 240, 12, 66, 24 }, "CHAIN", [this] { showDrawer (7, false); });
    chainButton.setToggleState (bank.chainOn.load(), dontSendNotification);
    chainButton.setTooltip ("Pattern chain: play slots in order, each for a number of bars");
    for (int i = 0; i < 16; ++i)
    {
        auto& b = button (panel, { 16 + (i % 8) * 62, 72 + (i / 8) * 40, 56, 34 },
                          String (i + 1).paddedLeft ('0', 2),
                          [this, i]
                          {
                              slotTarget = i;
                              processor.bank.requested.store (i);
                              showDrawer (4, false);
                          });
        b.setTooltip (names[i]);
        b.setToggleState (i == pattern, dontSendNotification);
    }
    auto& target = combo (panel, { 16, 154, 182, 25 }, names, slotTarget);
    target.onChange = [this, &target]
    {
        slotTarget = jmax (0, target.getSelectedItemIndex());
        MessageManager::callAsync ([safe = Component::SafePointer<Panel> (self())]
            { if (safe != nullptr && safe->drawer == 4) safe->showDrawer (4, false); });
    };
    auto& name = make<TextEditor> (panel, { 204, 154, 204, 25 });
    name.setText (names[slotTarget], false);
    button (panel, { 414, 154, 95, 25 }, "RENAME", [this, &name]
    {
        auto text = name.getText().trim().substring (0, 64);
        if (text.isEmpty()) return;
        { const std::lock_guard<std::mutex> guard (processor.bank.lock);
          processor.bank.slots[size_t (slotTarget)].name = text; }
        showDrawer (4, false);
    });
    button (panel, { 16, 186, 116, 25 }, "COPY", [this]
    {
        { const std::lock_guard<std::mutex> guard (processor.bank.lock);
          clipboard = processor.bank.slots[size_t (slotTarget)]; }
        showDrawer (4, false);
    });
    button (panel, { 140, 186, 116, 25 }, "PASTE", [this]
    {
        if (!clipboard) return;
        {
            const std::lock_guard<std::mutex> guard (processor.bank.lock);
            processor.bank.slots[size_t (slotTarget)] = *clipboard;
            if (slotTarget == pattern) processor.bank.editGeneration.fetch_add (1);
            processor.bank.sequenceRevision.fetch_add (1);
        }
        pullPattern();
        showDrawer (4, false);
    }).setEnabled (clipboard.has_value());
    button (panel, { 264, 186, 116, 25 }, "CLEAR", [this]
    {
        {
            const std::lock_guard<std::mutex> guard (processor.bank.lock);
            auto& slot = processor.bank.slots[size_t (slotTarget)];
            slot = {};
            slot.name = "Pattern " + String (slotTarget + 1);
            if (slotTarget == pattern) processor.bank.editGeneration.fetch_add (1);
            processor.bank.sequenceRevision.fetch_add (1);
        }
        pullPattern();
        showDrawer (4, false);
    });
    button (panel, { 388, 186, 121, 25 }, "SWITCH", [this] { processor.bank.requested.store (slotTarget); });
    label (panel, { 16, 217, 130, 20 }, "SWITCH QUANTIZE", darkOrange);
    label (panel, { 172, 217, 134, 20 }, "SOUND RECALL", darkOrange);
    auto& quant = combo (panel, { 16, 240, 147, 26 }, { "Immediate", "Next beat", "Next bar" }, bank.quantize.load());
    quant.onChange = [this, &quant] { processor.bank.quantize.store (jmax (0, quant.getSelectedItemIndex())); };
    auto& sound = combo (panel, { 172, 240, 147, 26 }, { "None", "Presets", "Full" }, bank.soundMode.load());
    sound.onChange = [this, &sound] { processor.bank.soundMode.store (jmax (0, sound.getSelectedItemIndex())); };
    auto& send = button (panel, { 328, 240, 181, 26 }, "PC OUT TO DAW", [this]
        { processor.bank.sendProgram.store (!processor.bank.sendProgram.load()); });
    send.setClickingTogglesState (true);
    send.setToggleState (bank.sendProgram.load(), dontSendNotification);
    send.setTooltip ("When on, clicking a slot also sends a Program Change on the plugin's MIDI output to your DAW, "
                     "so you can record slot changes in a clip. Nothing is sent to SEQTRAK.");
    for (int i = 0; i < 4; ++i)
    {
        const bool whole = i >= 2, exporting = i % 2 == 1;
        button (panel, { 16 + i * 124, 280, 121, 27 },
                String (exporting ? "EXPORT " : "IMPORT ") + (whole ? "BANK" : "SLOT"),
                [this, whole, exporting] { choosePatternFile (whole, exporting); });
    }
    label (panel, { 16, 311, 125, 16 }, "SEQUENCER MODE", darkOrange);
    label (panel, { 147, 311, 135, 16 }, "TRACK OVERRIDE", darkOrange);
    label (panel, { 287, 311, 112, 16 }, "SEQTRAK PATTERN", darkOrange);
    auto& mode = combo (panel, { 16, 329, 125, 24 }, { "PLUGIN", "HARDWARE" }, int (bank.hardwareMode.load()));
    mode.onChange = [this, &mode]
    {
        processor.bank.hardwareMode.store (mode.getSelectedItemIndex() == 1);
        MessageManager::callAsync ([safe = Component::SafePointer<Panel> (self())]
            { if (safe != nullptr && safe->drawer == 4) safe->showDrawer (4, false); });
    };
    auto& route = combo (panel, { 147, 329, 134, 24 }, { "Follow mode", "PLUGIN", "HARDWARE" },
                         bank.trackModes[size_t (track)].load());
    route.onChange = [this, &route]
    {
        processor.bank.trackModes[size_t (track)].store (jmax (0, route.getSelectedItemIndex()));
        MessageManager::callAsync ([safe = Component::SafePointer<Panel> (self())]
            { if (safe != nullptr && safe->drawer == 4) safe->showDrawer (4, false); });
    };
    seqtrak::HardwareTrack hardwareTrack;
    {
        const std::lock_guard<std::mutex> guard (bank.lock);
        hardwareTrack = bank.slots[size_t (slotTarget)].sequence.hardware[size_t (track)];
    }
    auto& hwPattern = combo (panel, { 287, 329, 105, 24 }, { "1", "2", "3", "4", "5", "6" }, hardwareTrack.pattern);
    hwPattern.onChange = [this, &hwPattern]
    {
        auto& b = processor.bank;
        const std::lock_guard<std::mutex> guard (b.lock);
        b.slots[size_t (slotTarget)].sequence.hardware[size_t (track)].pattern = jmax (0, hwPattern.getSelectedItemIndex());
        b.sequenceRevision.fetch_add (1);
    };
    auto& hwMute = button (panel, { 400, 329, 109, 24 }, "MUTE ON SEQTRAK", [this]
    {
        auto& b = processor.bank;
        const std::lock_guard<std::mutex> guard (b.lock);
        auto& muted = b.slots[size_t (slotTarget)].sequence.hardware[size_t (track)].muted;
        muted = !muted;
        b.sequenceRevision.fetch_add (1);
    });
    hwMute.setClickingTogglesState (true);
    hwMute.setToggleState (hardwareTrack.muted, dontSendNotification);
    // These only act when this track is played by SEQTRAK's own sequencer (HARDWARE mode).
    const bool hardwareTrackMode = seqtrak::usesHardware (bank, track);
    for (Component* c : { static_cast<Component*> (&hwPattern), static_cast<Component*> (&hwMute) })
    {
        c->setEnabled (hardwareTrackMode);
        c->setAlpha (hardwareTrackMode ? 1.0f : 0.4f);
    }
    const String hwTip = hardwareTrackMode ? String()
        : String ("Only used in HARDWARE mode: set Sequencer Mode or this track's override to HARDWARE.");
    hwPattern.setTooltip (hardwareTrackMode ? "SEQTRAK internal pattern (1-6) this slot plays on this track" : hwTip);
    hwMute.setTooltip (hardwareTrackMode ? "Mute this track on SEQTRAK when this slot is launched" : hwTip);
    label (panel, { 16, 354, 380, 22 }, patternStatus, darkOrange);
    auto& print = button (panel, { 400, 352, 109, 24 }, "PRINT", [this] { printSource = pattern; showDrawer (8, false); });
    print.setTooltip ("Write a slot's steps into one of SEQTRAK's own patterns, so it plays without the computer.");
}

void Panel::choosePatternFile (bool whole, bool exporting)
{
    patternFile = std::make_unique<FileChooser> (exporting ? "Export pattern JSON" : "Import pattern JSON",
        File::getSpecialLocation (File::userDocumentsDirectory).getChildFile (whole ? "SEQTRAK-Bank.json" : "SEQTRAK-Pattern.json"), "*.json");
    const auto mode = exporting ? FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting
                                : FileBrowserComponent::openMode;
    patternFile->launchAsync (mode | FileBrowserComponent::canSelectFiles,
        [safe = Component::SafePointer<Panel> (self()), whole, exporting, slot = slotTarget] (const FileChooser& chooser)
        {
            if (safe == nullptr || chooser.getURLResult().isEmpty()) return;
            auto& processor = safe->processor;
            const auto file = chooser.getResult();
            if (exporting)
            {
                var data;
                if (whole) data = processor.bankData();
                else
                {
                    const std::lock_guard<std::mutex> guard (processor.bank.lock);
                    data = seqtrak::object ({ {"format", "seqtrak-editor-slot"}, {"version", 1},
                        {"slot", seqtrak::encodePattern (processor.bank.slots[size_t (slot)])} });
                }
                safe->patternStatus = file.replaceWithText (JSON::toString (data)) ? "JSON exported." : "Could not write JSON.";
            }
            else
            {
                const auto data = file.getSize() <= 8000000 ? JSON::parse (file.loadFileAsString()) : var();
                bool valid = false;
                if (whole) valid = processor.loadBank (data, false);
                else if (data["format"].toString() == "seqtrak-editor-slot" && seqtrak::integer (data["version"], 1, 1))
                {
                    seqtrak::Pattern pattern;
                    valid = seqtrak::decodePattern (data["slot"], pattern, processor.voiceSchema, processor.hardware);
                    if (valid)
                    {
                        const std::lock_guard<std::mutex> guard (processor.bank.lock);
                        processor.bank.slots[size_t (slot)] = std::move (pattern);
                        if (slot == processor.bank.current.load()) processor.bank.editGeneration.fetch_add (1);
                        processor.bank.sequenceRevision.fetch_add (1);
                    }
                }
                if (valid) safe->pullPattern();
                safe->patternStatus = valid ? "JSON imported." : "Invalid SEQTRAK pattern JSON.";
            }
            if (safe->drawer == 4) safe->showDrawer (4, false);
            safe->repaint();
        });
}

Array<var> Panel::snapshots() const
{
    const std::lock_guard<std::mutex> lock (processor.snapshotMutex);
    const auto saved = JSON::parse (processor.snapshotJson);
    if (const auto* list = saved.getArray()) return *list;
    return {};
}

void Panel::saveSnapshots (const Array<var>& items)
{
    const std::lock_guard<std::mutex> lock (processor.snapshotMutex);
    processor.snapshotJson = JSON::toString (var (items), true);
}

var Panel::currentSnapshot (const String& name) const
{
    auto snapshot = std::make_unique<DynamicObject>();
    auto params = std::make_unique<DynamicObject>();
    const auto& defs = *schema[track].getArray();
    for (int i = 0; i < defs.size(); ++i)
        params->setProperty (defs[i]["id"].toString(), values[size_t (track)][size_t (i)]);
    snapshot->setProperty ("name", name.substring (0, 64));
    snapshot->setProperty ("part", track);
    snapshot->setProperty ("values", var (params.release()));
    return var (snapshot.release());
}

void Panel::chooseSnapshotFile (bool exporting)
{
    snapshotFile = std::make_unique<FileChooser> (exporting ? "Export Sound Lab snapshots" : "Import Sound Lab snapshots",
        File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("SEQTRAK-Sound-Lab.json"), "*.json");
    const auto mode = exporting ? FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting
                                : FileBrowserComponent::openMode;
    snapshotFile->launchAsync (mode | FileBrowserComponent::canSelectFiles,
        [safe = Component::SafePointer<Panel> (self()), exporting] (const FileChooser& chooser)
        {
            if (safe == nullptr || chooser.getURLResult().isEmpty()) return;
            const auto file = chooser.getResult();
            auto items = safe->snapshots();
            if (exporting)
            {
                auto data = std::make_unique<DynamicObject>();
                data->setProperty ("format", "seqtrak-sound-lab");
                data->setProperty ("version", 1);
                items.add (safe->currentSnapshot (tracks[safe->track] + " current"));
                data->setProperty ("presets", var (items));
                safe->snapshotStatus = file.replaceWithText (JSON::toString (var (data.release())))
                    ? "Snapshots exported." : "Could not write the snapshot file.";
            }
            else
            {
                const auto data = file.getSize() <= 2000000 ? JSON::parse (file.loadFileAsString()) : var();
                const auto* imported = data["presets"].getArray();
                bool valid = imported != nullptr && imported->size() <= 100
                    && data["format"].toString() == "seqtrak-sound-lab" && int (data["version"]) == 1;
                if (valid)
                    for (const auto& item : *imported)
                    {
                        const auto part = int (item["part"]);
                        if (!item["part"].isInt() || !isPositiveAndBelow (part, 11) || !item["name"].isString())
                        { valid = false; break; }
                        const auto& values = item["values"];
                        for (const auto& def : *safe->schema[part].getArray())
                        {
                            const auto id = def["id"].toString();
                            const auto v = values[Identifier (id)];
                            int max = int (def["max"]);
                            if (id.endsWith (".wave") && int (values[Identifier (id.upToLastOccurrenceOf (".", true, false) + "bank")]) == 1)
                                max = 2048;
                            if (!v.isInt() || int (v) < int (def["min"]) || int (v) > max) { valid = false; break; }
                        }
                        if (!valid) break;
                    }
                if (valid)
                {
                    for (const auto& item : *imported)
                    {
                        auto* object = item.getDynamicObject();
                        object->setProperty ("name", item["name"].toString().substring (0, 64));
                        items.add (item);
                    }
                    safe->saveSnapshots (items);
                }
                safe->snapshotStatus = valid ? "Snapshots imported. No MIDI sent." : "Invalid Sound Lab snapshot file.";
            }
            if (safe->drawer == 6) safe->showDrawer (6, false);
        });
}

void Panel::chainDrawer()
{
    auto& panel = *drawerPanel;
    auto& bank = processor.bank;
    navigation[4]->setToggleState (true, dontSendNotification);
    auto& on = button (panel, { 16, 48, 104, 26 }, "CHAIN ON", [this] {});
    on.setClickingTogglesState (true);
    on.setToggleState (bank.chainOn.load(), dontSendNotification);
    on.onClick = [this, &on] { processor.bank.chainOn.store (on.getToggleState()); };
    auto& loop = button (panel, { 128, 48, 80, 26 }, "LOOP", [this] {});
    loop.setClickingTogglesState (true);
    loop.setToggleState (bank.chainLoop.load(), dontSendNotification);
    loop.onClick = [this, &loop] { processor.bank.chainLoop.store (loop.getToggleState()); };
    button (panel, { 216, 48, 80, 26 }, "CLEAR", [this]
    {
        for (auto& c : processor.bank.chainSlots) c.store (-1);
        for (auto& c : processor.bank.chainBars) c.store (1);
        showDrawer (7, false);
    });
    button (panel, { 409, 48, 100, 26 }, "PATTERNS", [this] { showDrawer (4, false); });
    chainStatus = &label (panel, { 16, 80, 493, 20 }, {}, darkOrange);

    StringArray slots { CharPointer_UTF8 ("— end —") }, bars;
    StringArray names;
    {
        const std::lock_guard<std::mutex> guard (bank.lock);
        for (int i = 0; i < 16; ++i) names.add (String (i + 1).paddedLeft ('0', 2) + "  " + bank.slots[size_t (i)].name);
    }
    slots.addArray (names);
    for (int b = 1; b <= 64; ++b) bars.add (String (b) + (b == 1 ? " bar" : " bars"));
    for (int i = 0; i < seqtrak::Bank::chainSize; ++i)
    {
        const int x = 16 + (i / 8) * 250, y = 106 + (i % 8) * 33;
        chainNumbers[size_t (i)] = &label (panel, { x, y, 26, 26 }, String (i + 1), darkOrange);
        auto& slot = combo (panel, { x + 28, y, 130, 26 }, slots, bank.chainSlots[size_t (i)].load() + 1);
        slot.onChange = [this, i, &slot]
        {
            processor.bank.chainSlots[size_t (i)].store (slot.getSelectedItemIndex() - 1);
            shownChainPosition = -2;
        };
        auto& length = combo (panel, { x + 162, y, 78, 26 }, bars, jlimit (1, 64, bank.chainBars[size_t (i)].load()) - 1);
        length.onChange = [this, i, &length] { processor.bank.chainBars[size_t (i)].store (length.getSelectedItemIndex() + 1); };
    }
    updateChainStatus();
}

void Panel::printDrawer()
{
    auto& panel = *drawerPanel;
    auto& bank = processor.bank;
    navigation[4]->setToggleState (true, dontSendNotification);
    if (printSource < 0) printSource = pattern;
    StringArray names;
    {
        const std::lock_guard<std::mutex> guard (bank.lock);
        for (int i = 0; i < 16; ++i) names.add (String (i + 1).paddedLeft ('0', 2) + "  " + bank.slots[size_t (i)].name);
    }
    label (panel, { 16, 44, 493, 20 }, "Writes a slot into one of SEQTRAK's own patterns, so it plays without the computer.", darkOrange);
    label (panel, { 16, 72, 110, 26 }, "FROM SLOT", darkOrange);
    auto& source = combo (panel, { 128, 72, 220, 26 }, names, printSource);
    source.onChange = [this, &source] { printSource = jmax (0, source.getSelectedItemIndex()); };
    label (panel, { 16, 104, 150, 26 }, "TO SEQTRAK PATTERN", darkOrange);
    auto& dest = combo (panel, { 168, 104, 80, 26 }, { "1", "2", "3", "4", "5", "6" }, printDest);
    dest.onChange = [this, &dest] { printDest = jmax (0, dest.getSelectedItemIndex()); };
    button (panel, { 409, 72, 100, 26 }, "PATTERNS", [this] { showDrawer (4, false); });
    label (panel, { 16, 138, 120, 20 }, "TRACKS", darkOrange);
    for (int t = 0; t < 11; ++t)
    {
        auto& b = button (panel, { 16 + (t % 6) * 82, 160 + (t / 6) * 30, 78, 26 }, tracks[t], [] {});
        b.setClickingTogglesState (true);
        b.setToggleState (printTracks[size_t (t)], dontSendNotification);
        b.setTooltip ("Include " + tracks[t] + " in the print.");
        b.onClick = [this, t, &b] { printTracks[size_t (t)] = b.getToggleState(); };
    }
    button (panel, { 427, 190, 82, 26 }, "ALL", [this] { printTracks.fill (true); showDrawer (8, false); });
    label (panel, { 16, 226, 493, 36 },
           "Prints notes, velocity, length, micro timing and synth chords. Probability and flam stay in the editor. "
           "The chosen SEQTRAK pattern is overwritten on the selected tracks.", darkOrange);
    auto& go = button (panel, { 16, 272, 150, 30 }, "PRINT TO SEQTRAK", [this] { printToSeqtrak(); });
    go.setTooltip ("Stop SEQTRAK and your DAW first, then print.");
    printStatus = &label (panel, { 176, 272, 333, 30 },
                          "First time? Test on a SEQTRAK pattern you don't mind losing. Stop SEQTRAK and your DAW first.", cream);
    label (panel, { 16, 310, 493, 40 },
           "Tip: after printing, set SEQUENCER MODE to HARDWARE (or turn the editor's sequencer off) "
           "so the editor doesn't play the same notes on top of SEQTRAK.", darkOrange);
}

void Panel::printToSeqtrak()
{
    auto& hw = processor.hardware;
    if (!hw.verified)
    {
        printStatus->setText ("Not connected. Open CONNECTION, choose the SEQTRAK ports and press IDENTIFY.", dontSendNotification);
        return;
    }
    if (processor.bank.playing.load())
    {
        printStatus->setText ("Stop playback first (DAW and SEQTRAK), then print.", dontSendNotification);
        return;
    }
    seqtrak::Sequence seq;
    {
        const std::lock_guard<std::mutex> guard (processor.bank.lock);
        seq = processor.bank.slots[size_t (jlimit (0, 15, printSource))].sequence;
    }
    const int dest = jlimit (0, 5, printDest);
    int notesWritten = 0;
    hw.queueSysex (0x30, 0x40, 0x79, { 1 });                       // Pattern Select Mode 1: all six slots reachable
    for (int t = 0; t < 11; ++t)
    {
        if (!printTracks[size_t (t)]) continue;
        const int length = jlimit (1, 128, seq.lengths[size_t (t)]);
        hw.queueSysex (0x30, 0x50 + t, 0x0f, { dest });             // select the destination pattern (writes go there)
        hw.queueSysex (0x01, 0x10 + t, 0x37, { dest });             // clear it
        hw.queueSysex (0x30, 0x50 + t, 0x10 + dest, { jlimit (1, 8, (length + 15) / 16) });   // bar length
        hw.queueSysex (0x30, 0x50 + t, 0x16 + dest * 2, { length >> 7, length & 127 });        // step count
        for (int s = 0; s < length; ++s)
        {
            const auto& step = seq.step (t, s);
            if (!step.on) continue;
            // 120 ticks per step; the step centre is +60; plugin micro timing is % of a step.
            const int time = jlimit (0, length * 120 - 1, s * 120 + 60 + roundToInt (step.micro * 1.2));
            const int gate = jlimit (1, 16383, roundToInt (step.gate * 1.2));
            std::array<int, 5> chord {};
            const int voices = t >= 7 && t <= 9 ? seqtrak::stepNotes (step, chord) : (chord[0] = step.note, 1);
            for (int v = 0; v < voices; ++v)
            {
                const std::vector<int> event { time >> 7, time & 127, 0, chord[size_t (v)], step.velocity, 0, gate >> 7, gate & 127 };
                if (t < 7) hw.queueSysex (0x22, t << 4, 0, event);          // drum piano roll (global part 0-6)
                else       hw.queueSysex (0x72, (t - 7) << 4, 0, event);    // synth piano roll (local index 0-3)
                ++notesWritten;
            }
        }
    }
    printStatus->setText ("Printing... " + String (int (hw.queuedMessages())) + " messages (" + String (notesWritten)
                          + " notes). Don't start SEQTRAK yet.", dontSendNotification);
}

void Panel::updateChainStatus()
{
    if (chainStatus == nullptr) return;
    const auto& bank = processor.bank;
    const int position = bank.chainPosition.load();
    if (position == shownChainPosition) return;
    shownChainPosition = position;
    int entries = 0;
    while (entries < seqtrak::Bank::chainSize && bank.chainSlots[size_t (entries)].load() >= 0) ++entries;
    String text = entries == 0 ? "Pick slots below; the chain stops at the first empty step."
                : !bank.chainOn.load() ? String (entries) + " steps. Turn CHAIN ON and start playback."
                : position < 0 ? String (entries) + " steps. Starts with step 1 when playback starts."
                : "PLAYING STEP " + String (position + 1) + " OF " + String (entries) + "  /  SLOT "
                    + String (bank.chainSlots[size_t (position)].load() + 1).paddedLeft ('0', 2);
    chainStatus->setText (text, dontSendNotification);
    for (int i = 0; i < seqtrak::Bank::chainSize; ++i)
        if (auto* n = chainNumbers[size_t (i)])
        {
            n->setColour (Label::textColourId, i == position ? cream : darkOrange);
            n->setFont (face (15, i == position ? Font::bold : Font::plain));
        }
}

void Panel::snapshotsDrawer()
{
    auto& panel = *drawerPanel;
    auto& name = make<TextEditor> (panel, { 16, 49, 265, 27 });
    name.setTextToShowWhenEmpty ("Snapshot name", ink.withAlpha (0.65f));
    auto items = snapshots();
    button (panel, { 291, 49, 105, 27 }, "SAVE SNAPSHOT", [this, &name, items]() mutable
    {
        auto title = name.getText().trim();
        if (title.isEmpty()) title = tracks[track] + " discovery " + String (items.size() + 1);
        items.add (currentSnapshot (title));
        saveSnapshots (items);
        snapshotStatus = "Snapshot saved in plugin state.";
        showDrawer (6, false);
    }).setEnabled (!processor.hardware.busy());
    button (panel, { 404, 49, 105, 27 }, "DEEP EDIT", [this] { showDrawer (1); });
    StringArray titles;
    for (const auto& item : items) titles.add (item["name"].toString() + " / " + tracks[int (item["part"])]);
    auto& choice = combo (panel, { 16, 94, 493, 29 }, titles, titles.isEmpty() ? -1 : 0);
    choice.setTextWhenNothingSelected ("No saved snapshots");
    button (panel, { 16, 137, 112, 28 }, "LOAD", [this, &choice, items]
    {
        const auto index = choice.getSelectedItemIndex();
        if (!isPositiveAndBelow (index, items.size()) || processor.hardware.busy()) return;
        const auto& item = items.getReference (index);
        const auto part = int (item["part"]);
        const auto& defs = *schema[part].getArray();
        for (const auto& def : defs)
            if (!processor.hardware.canEdit (part, def))
            {
                snapshotStatus = "Read this track completely before loading its snapshot.";
                showDrawer (6, false);
                return;
            }
        std::vector<int> data;
        for (const auto& def : defs) data.push_back (int (item["values"][Identifier (def["id"].toString())]));
        checkpoint (part);
        applyVoice (part, data);
        track = part;
        voicePart = 0;
        fxFollowHardware = true;
        processor.hardware.selectTrack (track);
        refresh();
        snapshotStatus = "Snapshot loaded.";
        showDrawer (6, false);
    }).setEnabled (!items.isEmpty());
    button (panel, { 137, 137, 112, 28 }, "DELETE", [this, &choice, items]() mutable
    {
        if (!isPositiveAndBelow (choice.getSelectedItemIndex(), items.size())) return;
        items.remove (choice.getSelectedItemIndex());
        saveSnapshots (items);
        showDrawer (6, false);
    }).setEnabled (!items.isEmpty());
    button (panel, { 257, 137, 121, 28 }, "IMPORT JSON", [this] { chooseSnapshotFile (false); });
    button (panel, { 387, 137, 122, 28 }, "EXPORT JSON", [this] { chooseSnapshotFile (true); });
    label (panel, { 16, 181, 493, 31 }, snapshotStatus, darkOrange);
}

void Panel::connectionDrawer()
{
    auto& panel = *drawerPanel;
    auto& hw = processor.hardware;
    label (panel, { 16, 46, 195, 22 }, "MIDI INPUT", darkOrange);
    label (panel, { 218, 46, 195, 22 }, "MIDI OUTPUT", darkOrange);
    label (panel, { 420, 46, 90, 22 }, "DEVICE", darkOrange);
    const auto inputs = MidiInput::getAvailableDevices();
    const auto outputs = MidiOutput::getAvailableDevices();
    const auto makePort = [this, &panel] (int x, const auto& ports, const String& id)
    {
        StringArray names { "Select a port" };
        int selected = 0, preferred = 0;
        for (int i = 0; i < ports.size(); ++i)
        {
            names.add (ports[i].name);
            if (ports[i].identifier == id) selected = i + 1;
            if (preferred == 0 && ports[i].name.containsIgnoreCase ("seqtrak")) preferred = i + 1;
        }
        return &combo (panel, { x, 73, 193, 29 }, names, selected != 0 ? selected : preferred);
    };
    auto* input = makePort (16, inputs, hw.inputId);
    auto* output = makePort (218, outputs, hw.outputId);
    StringArray numbers;
    for (int i = 1; i <= 16; ++i) numbers.add (String (i));
    auto& device = combo (panel, { 420, 73, 89, 29 }, numbers, hw.device);
    const auto connect = [this, inputs, outputs, input, output, &device]
    {
        const int in = input->getSelectedItemIndex() - 1, out = output->getSelectedItemIndex() - 1;
        processor.hardware.connect (isPositiveAndBelow (in, inputs.size()) ? inputs[in].identifier : String(),
                                    isPositiveAndBelow (out, outputs.size()) ? outputs[out].identifier : String(),
                                    device.getSelectedItemIndex());
    };
    button (panel, { 16, 114, 105, 29 }, "IDENTIFY", [this, connect] { connect(); processor.hardware.identify(); });
    button (panel, { 128, 114, 120, 29 }, "READ HARDWARE", [this] { processor.hardware.read (track); })
        .setEnabled (hw.verified && !hw.busy());
    button (panel, { 255, 114, 119, 29 }, "READ ALL TRACKS", [this] { processor.hardware.readAll(); })
        .setEnabled (hw.verified && !hw.busy());
    button (panel, { 381, 114, 128, 29 }, "DISCONNECT", [this] { processor.hardware.connect ({}, {}, processor.hardware.device); });
    button (panel, { 16, 150, 105, 25 }, "REFRESH PORTS", [this] { showDrawer (5); });
    button (panel, { 128, 150, 120, 25 }, "PANIC", [this] { processor.hardware.panic(); });
    auto& mirror = button (panel, { 255, 150, 254, 25 }, "MIRROR NOTES TO HOST MIDI", [this]
        { processor.bank.mirror.store (!processor.bank.mirror.load()); });
    mirror.setClickingTogglesState (true);
    mirror.setToggleState (processor.bank.mirror.load(), dontSendNotification);
    auto& status = label (panel, { 16, 186, 493, 50 }, hw.status, darkOrange);
    status.setMinimumHorizontalScale (0.8f);
    label (panel, { 16, 241, 493, 23 }, "READ ALL PROGRESS   " + String (hw.progress()) + " / 11", darkOrange);
    auto& bar = make<Label> (panel, { 16, 269, 493, 7 });
    bar.setColour (Label::backgroundColourId, recess);
    if (hw.progress() > 0)
    {
        auto& completed = make<Label> (panel, { 16, 269, 493 * hw.progress() / 11, 7 });
        completed.setColour (Label::backgroundColourId, cream);
    }
    label (panel, { 16, 278, 493, 43 }, hw.summary, darkOrange).setJustificationType (Justification::topLeft);
    auto& b = processor.bank;
    auto& tempo = button (panel, { 16, 323, 172, 25 }, "SEND TEMPO TO SEQTRAK",
        [this] { processor.bank.sendTempo.store (!processor.bank.sendTempo.load()); });
    tempo.setClickingTogglesState (true);
    tempo.setToggleState (b.sendTempo.load(), dontSendNotification);
    auto& follow = button (panel, { 194, 323, 131, 25 }, "HARDWARE FOLLOW",
        [this] { processor.bank.hardwareFollow.store (!processor.bank.hardwareFollow.load()); });
    follow.setClickingTogglesState (true);
    follow.setToggleState (b.hardwareFollow.load(), dontSendNotification);
    label (panel, { 332, 305, 176, 16 }, "RECORD QUANTIZE", darkOrange);
    auto& record = combo (panel, { 332, 323, 177, 25 },
                          { "Leave unchanged", "Off", "1/32", "1/16T", "1/16", "1/8T", "1/8" },
                          b.recordQuantize.load() + 1);
    record.onChange = [this, &record] { processor.bank.recordQuantize.store (record.getSelectedItemIndex() - 1); };
}

void Panel::syncHardware()
{
    auto& hw = processor.hardware;
    if (hardwareSession != hw.session)
    {
        hardwareSession = hw.session;
        comparing = false;
        captured = {}; beforeCompare = {}; undo = {};
        fxFollowHardware = true;
    }
    if (hw.verified)
        for (int t = 0; t < 11; ++t)
        {
            const auto& known = hw.tracks[size_t (t)].known;
            const auto& defs = *schema[t].getArray();
            if (hw.readEpoch[size_t (t)] != readEpoch[size_t (t)])
            {
                readEpoch[size_t (t)] = hw.readEpoch[size_t (t)];
                captured[size_t (t)].clear(); undo[size_t (t)].clear(); beforeCompare[size_t (t)].clear();
                if (track == t) comparing = false;
            }
            for (int i = 0; i < defs.size(); ++i)
                if (auto it = known.find (seqtrak::address (defs[i])); it != known.end())
                    values[size_t (t)][size_t (i)] = it->second;
        }
    if (drawer == 0 && presetStatus != nullptr)
    {
        presetStatus->setText (hw.status, dontSendNotification);
        presetLoad->setEnabled (!hw.busy());
    }
    if (drawer == 1 || drawer == 2 || drawer == 3 || drawer == 5)
    {
        Point<int> scroll;
        if (drawerPanel != nullptr)
            for (auto* child : drawerPanel->getChildren())
                if (auto* viewport = dynamic_cast<Viewport*> (child)) scroll = viewport->getViewPosition();
        showDrawer (drawer, false);
        for (auto* child : drawerPanel->getChildren())
            if (auto* viewport = dynamic_cast<Viewport*> (child)) viewport->setViewPosition (scroll);
    }
    repaint();
}

void Panel::timerCallback()
{
    auto& bank = processor.bank;
    const auto switched = bank.switched.load();
    const auto restored = bank.restored.load();
    if (switched != seenSwitch || restored != seenRestore || pattern != bank.current.load())
    {
        seenSwitch = switched;
        seenRestore = restored;
        if (editedStep >= 0) closeDrawer();
        heldPad = -1;
        comparing = false;
        captured = {}; undo = {}; beforeCompare = {};
        pullPattern();
        repaint();
        if (drawer >= 0) showDrawer (drawer, false);
    }
    const int selected = bank.selectedTrack.exchange (-1);
    const int hit = bank.samplerPad.exchange (-1);
    if (bank.hardwareFollow.load() && hit >= 0)
    {
        flashedPad = hit;
        padFlashUntil = Time::getMillisecondCounterHiRes() + 250.0;
    }
    if (flashedPad >= 0 && (!bank.hardwareFollow.load() || Time::getMillisecondCounterHiRes() >= padFlashUntil))
        flashedPad = -1;
    if (bank.hardwareFollow.load() && selected >= 0 && selected != track)
    {
        if (comparing) toggleCompare();
        track = selected;
        voicePart = 0;
        fxFollowHardware = true;
        processor.hardware.selectTrack (track);
        if (lengthButton != nullptr) lengthButton->setButtonText ("LENGTH " + String (lengths[size_t (track)]));
        refresh();
        if (drawer >= 0) showDrawer (drawer, false);
    }
    if (drawer == 7) updateChainStatus();
    if (drawer == 8 && printStatus != nullptr && processor.hardware.queuedMessages() > 0)
        printStatus->setText ("Printing... " + String (int (processor.hardware.queuedMessages())) + " messages left. Don't start SEQTRAK yet.",
                              dontSendNotification);
    else if (drawer == 8 && printStatus != nullptr && printStatus->getText().startsWith ("Printing"))
        printStatus->setText ("Done. Press play on SEQTRAK to hear the printed pattern.", dontSendNotification);
    bpm = roundToInt (bank.displayTempo.load());
    playing = bank.playing.load();
    head = bank.heads[size_t (track)].load();
    if (bank.follow.load() && head >= 0 && editedStep < 0 && heldPad < 0) page = head / 16;
    // Only repaint when something visible changed (plus a slow safety refresh),
    // instead of redrawing the whole panel 30 times a second.
    int64 fingerprint = (int64 (bpm) << 40) ^ (int64 (head + 1) << 24) ^ (int64 (page) << 16) ^ (int64 (track) << 8)
                      ^ (int64 (playing) << 4) ^ (int64 (flashedPad + 1) << 52) ^ (int64 (editedStep + 1) << 32);
    for (int t = 0; t < 11; ++t)
        fingerprint ^= (int64 (mix[size_t (t)][0]) | (int64 (mix[size_t (t)][1]) << 1)) << (t * 2);
    if (fingerprint != lastFingerprint || ++safetyTicks >= 8)
    {
        lastFingerprint = fingerprint;
        safetyTicks = 0;
        repaint (33, 35, 807, 208);
        repaint (1169, 38, 180, 159);
        repaint (33, 246, 807, 174);
    }
    if (hardwareRevision != processor.hardware.revision && !isMouseButtonDownAnywhere())
    {
        hardwareRevision = processor.hardware.revision;
        syncHardware();
    }
}

class AboutContent final : public Component
{
public:
    AboutContent()
    {
        logo = Drawable::createFromImageData (BinaryData::TransistorLabs_svg, BinaryData::TransistorLabs_svgSize);
        addAndMakeVisible (email);
        addAndMakeVisible (site);
        email.setColour (HyperlinkButton::textColourId, ink);
        site.setColour (HyperlinkButton::textColourId, ink);
        setSize (385, 283);
    }

    void resized() override
    {
        email.setBounds (0, 193, getWidth(), 27);
        site.setBounds (0, 221, getWidth(), 27);
    }

    void paint (Graphics& g) override
    {
        g.fillAll (orange);
        g.setColour (cream);
        g.fillRect (16, 16, getWidth() - 32, 113);
        if (logo != nullptr)
        {
            const auto bounds = logo->getDrawableBounds();
            auto transform = RectanglePlacement (RectanglePlacement::centred)
                                 .getTransformToFit (bounds, { 30.0f, 21.0f, 325.0f, 102.0f });
            logo->draw (g, 1.0f, transform);
        }
        g.setColour (ink);
        g.setFont (face (24, Font::bold));
        g.drawText ("SEQTRAK EDITOR", 0, 139, getWidth(), 32, Justification::centred);
        g.setFont (face (13, Font::bold));
        g.drawText ("VERSION " JucePlugin_VersionString "  /  HARDWARE EDITOR", 0, 168, getWidth(), 22, Justification::centred);
    }

private:
    std::unique_ptr<Drawable> logo;
    HyperlinkButton email { "ticcoross@gmail.com", URL ("mailto:ticcoross@gmail.com") };
    HyperlinkButton site { "ticcoross.gumroad.com", URL ("https://ticcoross.gumroad.com/") };
};

class AboutOverlay final : public Component
{
public:
    AboutOverlay()
    {
        addAndMakeVisible (content);
        closeButton.setColour (TextButton::buttonColourId, darkOrange);
        closeButton.setColour (TextButton::textColourOffId, cream);
        closeButton.onClick = [this] { setVisible (false); };
        content.addAndMakeVisible (closeButton);
        setWantsKeyboardFocus (true);
    }
    void resized() override
    {
        content.setCentrePosition (getLocalBounds().getCentre());
        closeButton.setBounds (content.getWidth() - 40, content.getHeight() - 30, 30, 22);
    }
    void paint (Graphics& g) override
    {
        g.fillAll (Colours::black.withAlpha (0.55f));
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRect (content.getBounds().translated (6, 6));
    }
    void mouseDown (const MouseEvent& e) override
    {
        if (!content.getBounds().contains (e.getPosition())) setVisible (false);
    }
    bool keyPressed (const KeyPress& k) override
    {
        if (k == KeyPress::escapeKey) { setVisible (false); return true; }
        return false;
    }
private:
    AboutContent content;
    TextButton closeButton { "X" };
};

void Panel::about()
{
    if (!aboutWindow)
    {
        aboutWindow = std::make_unique<AboutOverlay>();
        addChildComponent (*aboutWindow);
    }
    aboutWindow->setBounds (getLocalBounds());
    aboutWindow->setVisible (true);
    aboutWindow->toFront (true);
    aboutWindow->grabKeyboardFocus();
}

void Panel::editStep (int index)
{
    // Use the orange area for step editing and keep the sequence visible.
    closeDrawer();
    editedStep = index;
    repaint();
    buildingDrawer = true;
    drawerPanel = &make<Component> (*this, { 841, 0, 525, 378 });
    auto& panel = *drawerPanel;
    auto& bg = make<Label> (panel, panel.getLocalBounds());
    bg.setColour (Label::backgroundColourId, orange);
    label (panel, { 16, 14, 375, 30 }, tracks[track] + "  /  STEP " + String (page * 16 + index + 1)).setFont (face (21, Font::bold));
    button (panel, { 438, 14, 70, 26 }, "CLOSE", [this] { closeDrawer(); });
    auto& step = stepAt (index);
    auto& toggle = button (panel, { 16, 59, 103, 28 }, "ACTIVE", [this, &step] { step.on = !step.on; publishSequence(); repaint(); });
    toggle.setClickingTogglesState (true);
    toggle.setToggleState (step.on, dontSendNotification);
    if (track >= 7 && track <= 9)   // synth and DX tracks: chord on this step
    {
        label (panel, { 136, 59, 60, 28 }, "CHORD", darkOrange);
        StringArray chordNames;
        for (const auto& c : seqtrak::chordTypes) chordNames.add (c.name);
        auto& chord = combo (panel, { 196, 59, 150, 28 }, chordNames, step.chord);
        chord.setTooltip ("Play a chord built on this step's MIDI note. PRINT TO SEQTRAK writes all its notes.");
        chord.onChange = [this, &step, &chord] { step.chord = jmax (0, chord.getSelectedItemIndex()); publishSequence(); repaint(); };
    }
    const auto add = [&] (int y, const String& name, int lo, int hi, int& value)
    {
        label (panel, { 16, y, 166, 35 }, name, darkOrange);
        auto& fader = slider (panel, { 179, y, 328, 35 }, name, lo, hi, value, Slider::LinearHorizontal, false);
        fader.setTextBoxStyle (Slider::TextBoxRight, false, 55, 25);
        fader.onValueChange = [this, &value, &fader] { value = int (fader.getValue()); publishSequence(); repaint(); };
    };
    add (98, "VELOCITY", 1, 127, step.velocity);
    add (141, "MIDI NOTE", 0, 127, step.note);
    add (184, "PROBABILITY (%)", 0, 100, step.probability);
    add (227, "MICRO TIMING (% STEP)", -50, 50, step.micro);
    add (270, "GATE (% STEP)", 1, 100, step.gate);
    add (313, "FLAM (% STEP)", 0, 50, step.flam);
    buildingDrawer = false;
}

class Editor final : public AudioProcessorEditor
{
public:
    explicit Editor (Processor& p) : AudioProcessorEditor (p), panel (p)
    {
        addAndMakeVisible (panel);
        setResizable (false, false);
        panel.onScale = [this] (int percent)
        {
            scale = float (percent) / 100.0f;
            setSize (roundToInt (width * scale), roundToInt (height * scale));
            resized();
        };
        setSize (width, height);
    }
    void resized() override { panel.setTransform (AffineTransform::scale (scale)); }

private:
    Panel panel;
    float scale = 1.0f;
};

AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }
} // namespace

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Processor(); }
