#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <mutex>

namespace seqtrak
{
struct Step
{
    bool on = false;
    int velocity = 100, note = 60, probability = 100, micro = 0, gate = 50;
    int flam = 0; // 0 = off, otherwise grace note this many % of a step before the hit
    int chord = 0; // 0 = single note, otherwise an entry of chordTypes (synth tracks)
};

// Step chords: semitone intervals above the step's note (root not listed).
struct ChordType { const char* name; std::array<int, 4> intervals; int count; };
inline const std::array<ChordType, 13> chordTypes {{
    { "Off", {}, 0 }, { "Major", { 4, 7 }, 2 }, { "Minor", { 3, 7 }, 2 }, { "7", { 4, 7, 10 }, 3 },
    { "Maj7", { 4, 7, 11 }, 3 }, { "m7", { 3, 7, 10 }, 3 }, { "m9", { 3, 7, 10, 14 }, 4 },
    { "Sus2", { 2, 7 }, 2 }, { "Sus4", { 5, 7 }, 2 }, { "Add9", { 4, 7, 14 }, 3 },
    { "Dim", { 3, 6 }, 2 }, { "Aug", { 4, 8 }, 2 }, { "Power", { 7, 12 }, 2 }
}};

// All notes a step plays (root first), clamped to MIDI range.
inline int stepNotes (const Step& step, std::array<int, 5>& out)
{
    int n = 0;
    out[size_t (n++)] = step.note;
    if (isPositiveAndBelow (step.chord, int (chordTypes.size())))
        for (int i = 0; i < chordTypes[size_t (step.chord)].count; ++i)
        {
            const int v = step.note + chordTypes[size_t (step.chord)].intervals[size_t (i)];
            if (v <= 127) out[size_t (n++)] = v;
        }
    return n;
}

using Grid = std::array<std::array<std::array<Step, 16>, 8>, 11>;
using Mix = std::array<std::array<bool, 2>, 11>;

struct HardwareTrack
{
    int pattern = 0;
    bool muted = false;
};

struct Sequence
{
    Grid grid {};
    std::array<int, 11> lengths { 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16 };
    Mix mix {};
    std::array<HardwareTrack, 11> hardware {};
    int swing = 50;

    const Step& step (int track, int index) const
    {
        return grid[size_t (track)][size_t (index / 16)][size_t (index % 16)];
    }
};

struct Sound
{
    int number = 0, effect = -1;
    String name;
    std::map<Address, int> voice;
    std::map<int, int> fx;
};

struct Pattern
{
    Sequence sequence;
    std::array<Sound, 11> sounds;
    std::array<int, 7> samples {};
    String name;
};

struct Bank
{
    std::mutex lock;
    std::array<Pattern, 16> slots;
    std::atomic<int> current { 0 }, requested { -1 }, hardwareProgram { -1 }, quantize { 0 }, soundMode { 0 };
    std::atomic<bool> mirror { true }, sendProgram { false }, follow { true }, standalonePlay { false };
    std::atomic<bool> hardwareMode { false }, sendTempo { false }, hardwareFollow { true };
    // 0 follows the mode switch, 1 forces plug-in notes, 2 forces hardware.
    std::array<std::atomic<int>, 11> trackModes {};
    std::atomic<int> recordQuantize { -1 }, selectedTrack { -1 }, samplerPad { -1 };
    std::array<std::atomic<int>, 11> displayedPatterns {};
    std::array<std::atomic<bool>, 11> displayedMutes {};
    std::atomic<int> tempo { 124 };
    std::atomic<float> displayTempo { 124 };
    std::atomic<bool> playing { false };
    std::array<std::atomic<int>, 11> heads {};
    std::atomic<unsigned> switched { 0 }, editGeneration { 0 }, restored { 0 }, sequenceRevision { 1 };

    // Pattern chain: up to 16 steps of (slot 0-15, bars 1-64); slot -1 ends the chain.
    static constexpr int chainSize = 16;
    std::array<std::atomic<int>, chainSize> chainSlots {}, chainBars {};
    std::atomic<bool> chainOn { false }, chainLoop { true }, chainBarQuantize { false };
    std::atomic<int> chainPosition { -1 };

    Bank()
    {
        for (int i = 0; i < 16; ++i) slots[size_t (i)].name = "Pattern " + String (i + 1);
        for (auto& mode : trackModes) mode.store (0);
        for (auto& mute : displayedMutes) mute.store (false);
        for (auto& pattern : displayedPatterns) pattern.store (-1);
        for (auto& head : heads) head.store (-1);
        for (auto& c : chainSlots) c.store (-1);
        for (auto& c : chainBars) c.store (1);
        for (int t = 0; t < 11; ++t)
            for (int s = 0; s < 16; ++s)
                slots[0].sequence.grid[size_t (t)][0][size_t (s)].on = (s + t) % (t == 3 || t == 4 ? 2 : 4) == 0;
    }
};

inline bool usesHardware (const Bank& bank, int track)
{
    const int mode = bank.trackModes[size_t (track)].load();
    return mode == 2 || (mode == 0 && bank.hardwareMode.load());
}

struct Clock
{
    bool playing = false;
    double ppq = 0, bpm = 124, barOrigin = 0, barLength = 4, beatLength = 1;
};

class Sequencer
{
public:
    void reset (int active, unsigned generation)
    {
        slot = active;
        editGeneration = generation;
        pending = -1;
        wasPlaying = false;
        mirrorWasOn = false;
        phase = 0;
        patternStart = -std::numeric_limits<double>::infinity();
        for (auto& note : notes) note = {};
    }

    template <typename Emit>
    void flush (int sample, Emit&& emit)
    {
        for (int t = 0; t < 11; ++t)
            stopNote (t, sample, emit);
    }

    template <typename Emit, typename Changed, typename Cancel, typename HardwareRange>
    void render (Bank& bank, const std::array<Sequence, 16>& sequences, Clock clock, int samples,
                 double rate, const MidiBuffer& input, int command, bool fromUI, Emit&& emit, Changed&& changed, Cancel&& cancel, HardwareRange&& hardwareRange)
    {
        const double ppqPerSample = clock.bpm / (60.0 * rate);
        if (samples <= 0 || ppqPerSample <= 0 || !std::isfinite (clock.ppq)) return;
        for (int t = 0; t < 11; ++t)
            if (usesHardware (bank, t)) stopNote (t, 0, emit);
        const bool mirror = bank.mirror.load();
        if (mirrorWasOn && !mirror)
            for (int t = 0; t < 11; ++t)
                if (notes[size_t (t)].mirrored)
                {
                    emit (false, t, notes[size_t (t)].number, 0, 0, true, false);
                    notes[size_t (t)].mirrored = false;
                }
        mirrorWasOn = mirror;

        const auto generation = bank.editGeneration.load();
        if (generation != editGeneration)
        {
            cancel();
            flush (0, emit);
            slot = bank.current.load();
            pending = -1;
            editGeneration = generation;
            wasPlaying = false;
        }
        const double tolerance = jmax (1.0e-7, ppqPerSample * 2.0);
        const bool jump = clock.playing && wasPlaying && std::abs (clock.ppq - expected) > tolerance;
        if (!clock.playing || !wasPlaying || jump)
        {
            if (wasPlaying) cancel();
            flush (0, emit);
            if (clock.playing)
            {
                launch = jump ? clock.ppq : nextBoundary (clock.ppq, clock.barOrigin, clock.barLength, tolerance);
                if (!wasPlaying) { phase = launch; patternStart = -std::numeric_limits<double>::infinity(); }
                if (jump) patternStart = -std::numeric_limits<double>::infinity();
            }
            if (pending >= 0) switchAt = quantized (bank, clock, clock.ppq, tolerance);
        }
        bank.playing.store (clock.playing);
        bank.displayTempo.store (float (clock.bpm));
        wasPlaying = clock.playing;
        expected = clock.ppq + samples * ppqPerSample;

        const auto requestSlot = [&] (int value, bool ui, int sample)
        {
            if (!isPositiveAndBelow (value, 16)) return;
            pending = value;
            announce = ui && bank.sendProgram.load();
            switchAt = quantized (bank, clock, clock.ppq + sample * ppqPerSample, tolerance);
        };
        if (command >= 0) requestSlot (command, fromUI, 0);
        int cursor = 0;
        const auto playRange = [&] (int first, int limit)
        {
            if (first < limit) hardwareRange (slot, first, limit, tolerance);
            const int chunk = jmax (1, int (jmin (double (samples), 16.0 / ppqPerSample)));
            while (first < limit)
            {
                const int end = first + jmin (chunk, limit - first);
                play (bank, sequences[size_t (slot)], clock, ppqPerSample, first, end, mirror, emit);
                first = end;
            }
        };
        const auto run = [&] (int limit)
        {
            if (pending >= 0)
            {
                const int switchSample = !clock.playing ? cursor : sampleFor (switchAt, clock.ppq, ppqPerSample, samples);
                if (switchSample < limit)
                {
                    const int boundary = jmax (cursor, switchSample);
                    playRange (cursor, boundary);
                    if (pending != slot)
                    {
                        flush (boundary, emit);
                        slot = pending;
                        // Pattern phase restarts on its change boundary.
                        phase = clock.ppq + boundary * ppqPerSample;
                        patternStart = phase;
                        bank.current.store (slot);
                        bank.switched.fetch_add (1);
                        changed (slot, boundary, announce);
                    }
                    pending = -1;
                    cursor = boundary;
                }
            }
            playRange (cursor, limit);
            cursor = limit;
        };
        for (const auto meta : input)
        {
            if (meta.numBytes != 2 || (meta.data[0] & 0xf0) != 0xc0 || meta.data[1] >= 16) continue;
            const auto sample = jlimit (cursor, samples - 1, meta.samplePosition);
            run (sample);
            requestSlot (meta.data[1], false, sample);
        }
        run (samples);
        const auto& sequence = sequences[size_t (slot)];
        for (int t = 0; t < 11; ++t)
        {
            const auto end = clock.ppq + (samples - 1) * ppqPerSample;
            const auto step = int64 (std::floor ((end - phase) * 4.0 + 1.0e-9));
            bank.heads[size_t (t)].store (!usesHardware (bank, t) && clock.playing && end >= jmax (launch, patternStart)
                ? wrapped (step, sequence.lengths[size_t (t)]) : -1);
        }
    }

private:
    struct Note { int number = -1; double off = 0; bool mirrored = false; std::array<int, 4> extra { -1, -1, -1, -1 }; };
    std::array<Note, 11> notes;
    int slot = 0, pending = -1;
    bool wasPlaying = false, announce = false, mirrorWasOn = false;
    unsigned editGeneration = 0;
    double switchAt = 0, expected = 0, launch = 0, phase = 0;
    double patternStart = -std::numeric_limits<double>::infinity();

    static int wrapped (int64 i, int length) { return int ((i % length + length) % length); }

    static double nextBoundary (double ppq, double origin, double length, double tolerance)
    {
        return origin + std::ceil ((ppq - origin - tolerance) / length) * length;
    }

public:
    static double quantized (const Bank& bank, Clock clock, double ppq, double tolerance)
    {
        // Chain switches always land on the next bar, whatever the switch-quantize setting.
        const int q = bank.chainBarQuantize.load() ? 2 : bank.quantize.load();
        if (!clock.playing || q == 0) return ppq;
        return nextBoundary (ppq, clock.barOrigin, q == 1 ? clock.beatLength : clock.barLength, tolerance);
    }

    static int sampleFor (double at, double start, double increment, int limit)
    {
        const auto sample = (at - start) / increment;
        if (sample <= 0) return 0;
        if (sample >= limit) return limit;
        return int (std::floor (sample + 0.5));
    }

private:
    static bool chance (int probability, int64 step, int track, int slot)
    {
        if (probability == 100) return true;
        if (probability == 0) return false;
        auto hash = uint64 (step) + uint64 (track + 11 * slot) * 0x9e3779b97f4a7c15ULL;
        hash = (hash ^ (hash >> 30)) * 0xbf58476d1ce4e5b9ULL;
        hash = (hash ^ (hash >> 27)) * 0x94d049bb133111ebULL;
        return (hash ^ (hash >> 31)) % 100 < unsigned (probability);
    }

    template <typename Emit>
    void stopNote (int track, int at, Emit& emit)
    {
        auto& note = notes[size_t (track)];
        if (note.number >= 0) emit (false, track, note.number, 0, at, note.mirrored, true);
        for (const int extra : note.extra)
            if (extra >= 0) emit (false, track, extra, 0, at, note.mirrored, true);
        note = {};
    }

    template <typename Emit>
    void play (const Bank& bank, const Sequence& seq, Clock clock, double increment, int first, int end, bool mirror, Emit& emit)
    {
        if (!clock.playing || first >= end) return;
        const auto startPpq = clock.ppq + first * increment;
        const auto endPpq = clock.ppq + end * increment;
        const auto audibleFrom = jmax (launch, patternStart);
        const auto firstGrid = int64 (std::floor ((startPpq - phase) * 4.0)) - 1;
        const auto lastGrid = int64 (std::ceil ((endPpq - phase) * 4.0)) + 1;
        bool solo = false;
        for (int t = 0; t < 11; ++t)
            if (!usesHardware (bank, t)) solo |= seq.mix[size_t (t)][1];
        for (int t = 0; t < 11; ++t)
        {
            auto& note = notes[size_t (t)];
            const auto mix = seq.mix[size_t (t)];
            if (usesHardware (bank, t) || mix[0] || (solo && !mix[1]))
            {
                stopNote (t, first, emit);
                continue;
            }
            const auto due = [&] (int through)
            {
                if (note.number >= 0)
                {
                    const int off = sampleFor (note.off, clock.ppq, increment, end);
                    if (off <= through) stopNote (t, jmax (first, off), emit);
                }
            };
            // Adjacent steps can exchange order at extreme microtiming. Sort each track's candidates.
            struct Hit { double ppq; const Step* step; bool grace; };
            std::array<Hit, 128> hits;
            size_t count = 0;
            for (auto i = firstGrid; i <= lastGrid && count < hits.size(); ++i)
            {
                const auto nominal = phase + double (i) * 0.25;
                const auto& step = seq.step (t, wrapped (i, seq.lengths[size_t (t)]));
                if (!step.on || !chance (step.probability, i, t, slot)) continue;
                const double swing = (i % 2 != 0 ? (seq.swing - 50) * 0.005 : 0.0);
                double at = nominal + swing + step.micro * 0.0025;
                if (nominal >= audibleFrom && at < audibleFrom) at = audibleFrom;
                if (nominal < patternStart || (phase == launch && nominal < launch)) continue;
                const auto inWindow = [&] (double when)
                {
                    return when >= audibleFrom && when >= startPpq - increment * 0.5 && when < endPpq - increment * 0.5;
                };
                // Flam: a softer grace note shortly before the main hit.
                if (step.flam > 0 && count < hits.size())
                {
                    const double graceAt = at - step.flam * 0.0025;
                    if (inWindow (graceAt)) hits[count++] = { graceAt, &step, true };
                }
                if (!inWindow (at) || count >= hits.size()) continue;
                hits[count++] = { at, &step, false };
            }
            std::sort (hits.begin(), hits.begin() + ptrdiff_t (count), [] (const Hit& a, const Hit& b) { return a.ppq < b.ppq; });
            for (size_t i = 0; i < count; ++i)
            {
                const auto& hit = hits[i];
                const auto at = jmax (first, sampleFor (hit.ppq, clock.ppq, increment, end));
                due (at);
                stopNote (t, at, emit);
                if (hit.grace)
                {
                    const int graceVelocity = jlimit (1, 127, roundToInt (hit.step->velocity * 0.6));
                    emit (true, t, hit.step->note, graceVelocity, at, mirror, true);
                    note = { hit.step->note, hit.ppq + hit.step->flam * 0.0025, mirror };
                    continue;
                }
                std::array<int, 5> chordNotes {};
                const int voices = stepNotes (*hit.step, chordNotes);
                for (int v = 0; v < voices; ++v)
                    emit (true, t, chordNotes[size_t (v)], hit.step->velocity, at, mirror, true);
                note = { hit.step->note, hit.ppq + hit.step->gate * 0.0025, mirror };
                for (int v = 1; v < voices; ++v) note.extra[size_t (v - 1)] = chordNotes[size_t (v)];
            }
            due (end - 1);
        }
    }
};
} // namespace seqtrak
