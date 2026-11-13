#pragma once

#include <array>
#include <deque>
#include <map>
#include <mutex>
#include <vector>

namespace seqtrak
{
using namespace juce;
using Address = std::array<int, 3>;
using Bytes = std::vector<uint8>;

inline Bytes request (int device, Address a, bool bulk = false)
{
    return { 240, 67, uint8 ((bulk ? 32 : 48) + device), 127, 28, 12,
             uint8 (a[0]), uint8 (a[1]), uint8 (a[2]), 247 };
}

inline Bytes write (int device, Address a, int value, int size)
{
    auto bytes = request (device, a);
    bytes[2] = uint8 (16 + device);
    bytes.pop_back();
    if (size == 2) bytes.push_back (uint8 ((value >> 7) & 127));
    bytes.push_back (uint8 (value & 127));
    bytes.push_back (247);
    return bytes;
}

inline std::array<Bytes, 3> factory (int part, int number, int pad)
{
    const auto index = number - 1;
    const auto channel = part == 10 ? pad : part;
    return {{ { uint8 (176 + channel), 0, uint8 (part == 10 ? 62 : 63) },
              { uint8 (176 + channel), 32, uint8 (index / 128) },
              { uint8 (192 + channel), uint8 (index % 128) } }};
}

inline Address address (const var& def)
{
    const auto& a = def["addr"];
    return { int (a[0]), int (a[1]), int (a[2]) };
}

inline int sizeOf (const var& def) { return def["bytes"].isVoid() ? 1 : int (def["bytes"]); }
inline Address fxAddress (int part, int offset) { return { 0x31, 0x20 + part, offset }; }

struct Packet
{
    Address address {};
    Bytes data;
    bool bulk = false;
};

inline bool decode (const Bytes& d, int device, Packet& p)
{
    if (d.size() < 10 || d.front() != 240 || d.back() != 247 || d[1] != 67
        || d[3] != 127 || d[4] != 28 || (d[2] & 15) != device) return false;
    for (size_t i = 1; i + 1 < d.size(); ++i)
        if (d[i] >= 128) return false;
    const auto type = d[2] & 240;
    size_t offset = 0, end = d.size() - 1;
    if (type == 16 && d[5] == 12) offset = 6;
    else if (type == 0 && d.size() >= 13 && d[7] == 12)
    {
        if (d.size() != size_t (d[5] * 128 + d[6] + 9)) return false;
        int checksum = 0;
        for (size_t i = 7; i < end; ++i) checksum += d[i];
        if (checksum % 128 != 0) return false;
        offset = 8;
        --end;
    }
    if (offset == 0 || end < offset + 3) return false;
    p = { { d[offset], d[offset + 1], d[offset + 2] },
          Bytes (d.begin() + ptrdiff_t (offset + 3), d.begin() + ptrdiff_t (end)), type == 0 };
    return true;
}

inline String soundName (const Bytes& d)
{
    if (d.size() < 88) return {};
    std::array<char, 77> decoded {};
    int count = 0;
    for (int offset = 0; offset < 88; offset += 8)
        for (int i = 0; i < 7; ++i)
        {
            const auto value = d[size_t (offset + i + 1)] | (((d[size_t (offset)] >> i) & 1) << 7);
            if (value == 0) return String::fromUTF8 (decoded.data(), count).trim();
            decoded[size_t (count++)] = char (value);
        }
    return String::fromUTF8 (decoded.data(), count).trim();
}

class Hardware final : private MidiInputCallback, private Timer
{
public:
    struct Track
    {
        std::map<Address, int> known;
        String name;
        int number = 0, effect = -1;
        unsigned revision = 0;
        std::map<int, int> fx;
    };
    static constexpr std::array<int, 11> readOrder { 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 10 };

    Hardware (var voiceSchema, var fxCatalogue, StringArray trackNames)
        : schema (std::move (voiceSchema)), effects (std::move (fxCatalogue)), names (std::move (trackNames))
    {
        refreshAt.fill (0);
        bank.fill (-1);
    }

    struct ScheduledNote { double time; uint8 status, note, velocity; };

    void scheduleNote (bool on, int part, int note, int velocity, double time)
    {
        if (!usbReady.load (std::memory_order_relaxed)) return;
        const auto write = usbNotesFifo.write (1);
        if (write.blockSize1 == 1)
            usbNotes[size_t (write.startIndex1)] = { time, uint8 ((on ? 144 : 128) + part), uint8 (note), uint8 (velocity) };
        else usbOverflow.store (true);
    }

    void cancelScheduledNotes()
    {
        if (!usbReady.load (std::memory_order_relaxed)) return;
        const auto write = usbNotesFifo.write (1);
        if (write.blockSize1 == 1) usbNotes[size_t (write.startIndex1)] = { 0, 0, 0, 0 };
        else usbOverflow.store (true);
    }

    void recallSound (int part, int number, const std::map<Address, int>& voice,
                      int effect, const std::map<int, int>& fx, bool full, int pad = 0)
    {
        if (!verified) return;
        if (number > 0)
            for (auto& bytes : factory (part, number, pad)) enqueue (std::move (bytes));
        if (!full) return;
        const auto& defs = *schema[part].getArray();
        for (const auto bankFirst : { true, false })
            for (const auto& def : defs)
            {
                if (def["id"].toString().endsWith (".bank") != bankFirst) continue;
                const auto a = address (def);
                if (auto it = voice.find (a); it != voice.end()) enqueue (write (device, a, it->second, sizeOf (def)));
            }
        if (!effectFor (effect).isVoid())
        {
            enqueue (write (device, fxAddress (part, 0), effect, 2));
            if (effect != 0) enqueue (write (device, { 0x31, 0x10 + part, 0x29 }, 3, 1));
            for (const auto& item : fx) enqueue (write (device, fxAddress (part, item.first), item.second, 2));
        }
    }

    void beginRecall()
    {
        outbox.clear();
        activeRead = allIndex = -1;
        pending.clear();
        stopAudition();
        for (int part = 0; part < 11; ++part)
        {
            auto& track = tracks[size_t (part)];
            track = {};
            ++readEpoch[size_t (part)];
        }
        ++revision;
    }

    std::function<void (int)> onProgramChange;
    std::function<void (int, int, int)> onPresetLoad;
    std::function<void()> onSequenceTick;

    ~Hardware() override
    {
        stopTimer();
        stopAudition();
        stopUsb();
        if (input != nullptr) input->stop();
        input.reset();
        output.reset();
    }

    void connect (const String& in, const String& out, int deviceNumber)
    {
        stopAudition();
        stopUsb();
        if (input != nullptr) input->stop();
        input.reset();
        output.reset();
        { const std::lock_guard<std::mutex> lock (inboxLock); inbox.clear(); }
        reset();
        device = jlimit (0, 15, deviceNumber);
        inputId = in; outputId = out;
        if (in.isNotEmpty()) input = MidiInput::openDevice (in, this);
        if (out.isNotEmpty()) output = MidiOutput::openDevice (out);
        if (output != nullptr) output->startBackgroundThread();
        if (input != nullptr) input->start();
        status = ready() ? "Ports open. Identify SEQTRAK to enable editing."
                         : "Select and open both SEQTRAK USB MIDI ports.";
        ++revision;
        if (input != nullptr || output != nullptr) startTimer (1);
        else stopTimer();
    }

    bool ready() const { return input != nullptr && output != nullptr; }
    bool busy() const { return activeRead >= 0 || allIndex >= 0; }

    void identify()
    {
        if (!ready()) return;
        stopAudition();
        reset();
        identification = now() + 2500;
        status = "Waiting for SEQTRAK identity reply...";
        transmit ({ 240, 126, 127, 6, 1, 247 });
        ++revision;
    }

    void selectTrack (int part)
    {
        selected = part;
        if (verified && tracks[size_t (part)].name.isEmpty()) refreshAt[size_t (part)] = now() + 250;
    }

    bool canEdit (int part, const var& def) const
    {
        if (!verified) return true;
        const auto& known = tracks[size_t (part)].known;
        if (busy() || known.count (address (def)) == 0) return false;
        const auto id = def["id"].toString();
        if (!id.endsWith (".bank") && !id.endsWith (".wave")) return true;
        const auto counterpart = id.upToLastOccurrenceOf (".", true, false)
                               + (id.endsWith (".bank") ? "wave" : "bank");
        for (const auto& p : *schema[part].getArray())
            if (p["id"].toString() == counterpart) return known.count (address (p)) != 0;
        return false;
    }

    void setVoice (int part, const var& def, int value)
    {
        if (!verified || !canEdit (part, def)) return;
        const auto a = address (def);
        tracks[size_t (part)].known[a] = value;
        enqueue (write (device, a, value, sizeOf (def)));
    }

    void read (int part, int number = 0, int pad = 0)
    {
        if (!verified || busy()) return;
        if (number != 0 && !outbox.empty())
        {
            status = "Wait for queued MIDI edits before loading a preset.";
            ++revision;
            return;
        }
        beginRead (part, number, pad);
    }

    void readAll()
    {
        if (!verified || busy()) return;
        soundIncomplete.clear();
        summary.clear();
        allIndex = 0;
        beginRead (readOrder[0], 0, 0);
    }

    int progress() const { return allIndex < 0 ? lastProgress : jmin (allIndex, 11); }

    const var& effectFor (int type) const
    {
        for (const auto& effect : *effects.getArray())
            if (int (effect["value"]) == type) return effect;
        return empty;
    }

    void readFx (int part)
    {
        if (!verified || busy()) return;
        auto& t = tracks[size_t (part)];
        t.effect = -1; t.fx.clear();
        enqueue (request (device, fxAddress (part, 0)));
        status = "Reading Track FX on " + names[part] + "...";
        ++revision;
    }

    void applyFx (int part, int type)
    {
        if (!verified || busy() || effectFor (type).isVoid()) return;
        enqueue (write (device, fxAddress (part, 0), type, 2));
        if (type != 0) enqueue (write (device, { 0x31, 0x10 + part, 0x29 }, 3, 1));
        readFx (part);
    }

    void setFx (int part, int type, int offset, int value)
    {
        if (!verified || busy()) return;
        auto& t = tracks[size_t (part)];
        if (t.effect != type || t.fx.count (offset) == 0) return;
        if (t.fx[offset] == value) return;
        t.fx[offset] = value;
        t.revision = ++revision;
        enqueue (write (device, fxAddress (part, offset), value, 2));
    }

    void audition (int part, int note)
    {
        if (!verified) return;
        stopAudition();
        auditionPart = part; auditionNote = note;
        transmit ({ uint8 (144 + part), uint8 (note), 85 });
    }

    void stopAudition()
    {
        if (auditionPart < 0) return;
        transmit ({ uint8 (128 + auditionPart), uint8 (auditionNote), 0 });
        auditionPart = -1;
    }

    void panic()
    {
        if (output != nullptr) output->clearAllPendingMessages();
        const auto discarded = usbNotesFifo.read (usbNotesFifo.getNumReady());
        (void) discarded;
        outbox.clear();
        if (allIndex >= 0) summary = "Read all tracks canceled.";
        activeRead = allIndex = -1;
        pending.clear();
        stopAudition();
        if (verified)
            for (int ch = 0; ch < 11; ++ch)
            {
                transmit ({ uint8 (176 + ch), 64, 0 });
                transmit ({ uint8 (176 + ch), 120, 0 });
            }
        status = "Queued edits cleared. All Sound Off sent when connected.";
        ++revision;
    }

    String inputId, outputId, status { "Demo mode. Select SEQTRAK USB MIDI ports." }, summary;
    std::array<Track, 11> tracks;
    int device = 0;
    bool verified = false;
    unsigned revision = 0, session = 0;
    std::array<unsigned, 11> readEpoch {};

private:
    static double now() { return Time::getMillisecondCounterHiRes(); }
    void enqueue (Bytes bytes) { if (verified && output != nullptr) outbox.push_back (std::move (bytes)); }
    void transmit (const Bytes& bytes)
    {
        if (output == nullptr) return;
        if (bytes.size() == 2 && (bytes[0] & 240) == 192)
            sentPrograms[size_t (bytes[0] & 15)] = { bytes[1], now() };
        output->sendMessageNow (MidiMessage (bytes.data(), int (bytes.size()), 0.0));
    }

    void reset()
    {
        ++session;
        usbReady.store (false);
        verified = false; identification = 0;
        tracks = {}; bank.fill (-1); refreshAt.fill (0);
        outbox.clear(); pending.clear();
        activeRead = allIndex = -1; lastProgress = 0; summary.clear();
    }

    void handleIncomingMidiMessage (MidiInput*, const MidiMessage& msg) override
    {
        const auto* data = msg.getRawData();
        const auto size = msg.getRawDataSize();
        if (size <= 0 || size > 32768) return;
        const auto type = data[0] & 240;
        if (data[0] != 240 && type != 176 && type != 192) return;
        const std::lock_guard<std::mutex> lock (inboxLock);
        if (inbox.size() < 4096) inbox.emplace_back (data, data + size);
    }

    void requestSound (int part)
    {
        if (part < 10) enqueue (request (device, { 1, 0x10 + part, 0x33 }));
        enqueue (request (device, { 0x11, 2, part }, true));
    }

    void beginRead (int part, int number, int pad)
    {
        outbox.clear();
        auto& t = tracks[size_t (part)];
        ++readEpoch[size_t (part)];
        t.known.clear(); t.name.clear(); t.number = 0;
        if (number != 0)
        {
            stopAudition();
            t.effect = -1; t.fx.clear();
            for (auto& bytes : factory (part, number, pad)) enqueue (std::move (bytes));
            if (onPresetLoad) onPresetLoad (part, number, pad);
        }
        refreshAt[size_t (part)] = 0;
        pending.clear();
        for (const auto& def : *schema[part].getArray()) pending.emplace (address (def), def);
        activeRead = part; attempt = 0; dumpRequested = number == 0;
        if (dumpRequested) requestSound (part);
        deadline = now() + (dumpRequested ? 1500 : 350);
        status = "Reading " + names[part] + ": requesting sound dump...";
        ++revision;
    }

    void finishRead (bool complete)
    {
        const auto part = activeRead;
        if (complete) status = names[part] + ": hardware read complete.";
        else
        {
            const int count = schema[part].getArray()->size();
            StringArray missing;
            for (const auto& entry : pending)
            {
                if (missing.size() == 3) break;
                StringArray hex;
                for (int byte : entry.first) hex.add (String::toHexString (byte).paddedLeft ('0', 2).toUpperCase());
                missing.add (entry.second["label"].toString() + " [" + hex.joinIntoString (" ") + "]");
            }
            status = names[part] + ": read " + String (count - int (pending.size())) + "/" + String (count)
                     + ". Unread: " + missing.joinIntoString (", ");
            if (allIndex >= 0) soundIncomplete.add (names[part]);
        }
        pending.clear();
        activeRead = -1;
        if (allIndex >= 0) { ++allIndex; deadline = now() + 100; }
        ++revision;
    }

    void advanceRead (double time)
    {
        if (time < deadline) return;
        if (activeRead >= 0)
        {
            if (!dumpRequested)
            {
                dumpRequested = true;
                requestSound (activeRead);
                deadline = time + 1500;
            }
            else if (attempt < 4)
            {
                ++attempt;
                for (const auto& def : *schema[activeRead].getArray())
                {
                    const auto a = address (def);
                    if (pending.count (a) != 0) enqueue (request (device, a));
                }
                status = "Reading " + names[activeRead] + ": " + String (int (pending.size())) + " parameters...";
                deadline = time + jmax (1200, int (pending.size()) * 20 + 800);
                ++revision;
            }
            else finishRead (false);
            return;
        }
        if (allIndex < 0) return;
        if (allIndex < 11) { beginRead (readOrder[size_t (allIndex)], 0, 0); return; }
        if (allIndex == 11)
        {
            for (const auto part : readOrder)
            {
                auto& t = tracks[size_t (part)];
                t.effect = -1; t.fx.clear();
                enqueue (request (device, fxAddress (part, 0)));
            }
            status = "Reading Track FX for all 11 tracks...";
            ++allIndex;
            deadline = time + 6000;
            ++revision;
            return;
        }
        StringArray fxIncomplete;
        for (int part = 0; part < 11; ++part)
        {
            const auto& t = tracks[size_t (part)];
            const auto* parameters = effectFor (t.effect)["params"].getArray();
            bool complete = parameters != nullptr;
            if (parameters != nullptr)
                for (const auto& param : *parameters)
                    if (t.fx.count (int (param["offset"])) == 0) complete = false;
            if (!complete) fxIncomplete.add (names[part]);
        }
        summary = "Sound " + String (11 - soundIncomplete.size()) + "/11 | FX " + String (11 - fxIncomplete.size()) + "/11";
        if (!soundIncomplete.isEmpty()) summary += "\nSound incomplete: " + soundIncomplete.joinIntoString (", ");
        if (!fxIncomplete.isEmpty()) summary += "\nFX incomplete: " + fxIncomplete.joinIntoString (", ");
        status = "Read all tracks finished.";
        lastProgress = 11; allIndex = -1;
        ++revision;
    }

    void receiveFx (int part, int offset, const Bytes& data)
    {
        if (data.size() < 2) return;
        auto& t = tracks[size_t (part)];
        const int value = data[0] * 128 + data[1];
        if (offset == 0)
        {
            t.effect = effectFor (value).isVoid() ? -1 : value; t.fx.clear();
            if (const auto* parameters = effectFor (value)["params"].getArray())
            {
                if (activeRead < 0)
                    for (const auto& p : *parameters) enqueue (request (device, fxAddress (part, int (p["offset"]))));
            }
        }
        else if (const auto* parameters = effectFor (t.effect)["params"].getArray())
        {
            for (const auto& p : *parameters)
                if (int (p["offset"]) == offset && value >= int (p["min"]) && value <= int (p["max"]))
                    t.fx[offset] = value;
        }
        t.revision = ++revision;
    }

    void receive (const Bytes& bytes)
    {
        if (identification != 0 && bytes.size() >= 11 && bytes[0] == 240 && bytes[1] == 126
            && bytes.back() == 247 && bytes[3] == 6 && bytes[4] == 2 && bytes[5] == 67
            && bytes[6] == 0 && bytes[7] == 65 && bytes[8] == 100 && bytes[9] == 6)
        {
            identification = 0; verified = true;
            usbReady.store (true);
            status = "SEQTRAK identified. Read hardware to synchronize a track.";
            selectTrack (selected);
            ++revision;
            return;
        }
        if (!verified || bytes.empty()) return;
        if (bytes[0] != 240)
        {
            const auto type = bytes[0] & 240;
            const auto channel = bytes[0] & 15;
            if (type == 176 && bytes.size() == 3 && bytes[1] == 0) bank[size_t (channel)] = bytes[2];
            if (type == 192 && bytes.size() == 2)
            {
                const auto time = now();
                const auto last = sentPrograms[size_t (channel)];
                const bool echo = last.first == bytes[1] && time - last.second < 500;
                const auto bankValue = bank[size_t (channel)];
                if (!echo && bytes[1] < 16 && onProgramChange)
                    onProgramChange (bytes[1]);
                const int part = bankValue == 62 ? 10 : channel;
                if (part > 10) return;
                if (!echo)
                {
                    auto& changed = tracks[size_t (part)];
                    changed = {};
                    ++readEpoch[size_t (part)];
                    changed.revision = ++revision;
                }
                refreshAt[size_t (part)] = now() + 250;
                ++revision;
            }
            return;
        }

        Packet packet;
        if (!decode (bytes, device, packet)) return;
        const auto [hi, mid, low] = packet.address;
        const auto& data = packet.data;
        if (!packet.bulk && hi == 1 && mid >= 0x10 && mid < 0x1a && low == 0x33 && data.size() == 2)
        {
            tracks[size_t (mid - 0x10)].number = data[0] * 128 + data[1] + 1;
            tracks[size_t (mid - 0x10)].revision = ++revision;
            return;
        }
        if (!packet.bulk && hi == 0x31 && mid >= 0x20 && mid <= 0x2a)
        {
            receiveFx (mid - 0x20, low, data);
            return;
        }
        if (packet.bulk && hi == 0x31 && mid <= 10 && low == 0 && data.size() >= 88)
        {
            const auto name = soundName (data);
            if (name.isNotEmpty()) { tracks[size_t (mid)].name = name; tracks[size_t (mid)].revision = ++revision; }
            return;
        }
        for (int part = 0; part < 11; ++part)
            for (const auto& def : *schema[part].getArray())
            {
                const auto a = address (def);
                const int offset = a[2] - low, count = sizeOf (def);
                if (a[0] != hi || a[1] != mid || offset < 0 || size_t (offset + count) > data.size()) continue;
                const int value = count == 2 ? data[size_t (offset)] * 128 + data[size_t (offset + 1)]
                                             : data[size_t (offset)];
                if (value < int (def["min"]) || value > int (def["max"])) continue;
                tracks[size_t (part)].known[a] = value;
                if (part == activeRead) pending.erase (a);
                tracks[size_t (part)].revision = ++revision;
            }
        if (activeRead >= 0 && pending.empty()) finishRead (true);
    }

    void stopUsb()
    {
        usbReady.store (false);
        if (output == nullptr) return;
        output->stopBackgroundThread();
        output->clearAllPendingMessages();
        for (int part = 0; part < 11; ++part) transmit ({ uint8 (176 + part), 120, 0 });
    }

    void drainNotes (double time)
    {
        const auto read = usbNotesFifo.read (usbNotesFifo.getNumReady());
        if (output == nullptr || !verified) return;
        if (usbOverflow.exchange (false))
        {
            output->clearAllPendingMessages();
            for (int part = 0; part < 11; ++part) transmit ({ uint8 (176 + part), 120, 0 });
            return;
        }
        MidiBuffer buffer;
        const auto add = [&] (int start, int count)
        {
            for (int i = start; i < start + count; ++i)
            {
                const auto& note = usbNotes[size_t (i)];
                if (note.status == 0)
                {
                    buffer.clear();
                    output->clearAllPendingMessages();
                    for (int part = 0; part < 11; ++part) transmit ({ uint8 (176 + part), 120, 0 });
                    continue;
                }
                const uint8 data[] { note.status, note.note, note.velocity };
                buffer.addEvent (data, 3, jmax (0, roundToInt ((note.time - time) * 1000.0)));
            }
        };
        add (read.startIndex1, read.blockSize1);
        add (read.startIndex2, read.blockSize2);
        if (!buffer.isEmpty()) output->sendBlockOfMessages (buffer, time + 0.25, 1000000.0);
    }

    void timerCallback() override
    {
        const auto time = now();
        if (onSequenceTick) onSequenceTick();
        drainNotes (time);
        if (time >= nextPortCheck)
        {
            nextPortCheck = time + 1000;
            const auto exists = [] (const Array<MidiDeviceInfo>& ports, const String& id)
            {
                for (const auto& port : ports) if (port.identifier == id) return true;
                return false;
            };
            if ((input != nullptr && !exists (MidiInput::getAvailableDevices(), inputId))
                || (output != nullptr && !exists (MidiOutput::getAvailableDevices(), outputId)))
            {
                connect ({}, {}, device);
                status = "MIDI port disconnected. Select ports and identify again.";
                return;
            }
        }
        std::deque<Bytes> received;
        { const std::lock_guard<std::mutex> lock (inboxLock); inbox.swap (received); }
        for (const auto& bytes : received) receive (bytes);
        if (identification != 0 && time >= identification)
        {
            identification = 0;
            status = "No SEQTRAK identity reply. Check USB ports and MIDI settings.";
            ++revision;
        }
        advanceRead (time);
        if (!busy())
            for (int part = 0; part < 11; ++part)
            {
                auto& next = refreshAt[size_t (part)];
                if (next != 0 && time >= next) { next = 0; requestSound (part); }
            }
        if (!outbox.empty() && time >= nextTransmit)
        {
            nextTransmit = time + 12;
            auto bytes = std::move (outbox.front());
            outbox.pop_front();
            transmit (bytes);
        }
    }

    std::atomic<bool> usbReady { false }, usbOverflow { false };
    std::array<ScheduledNote, 8192> usbNotes {};
    AbstractFifo usbNotesFifo { int (usbNotes.size()) };
    std::array<std::pair<int, double>, 16> sentPrograms {};
    double nextTransmit = 0;
    const var schema, effects, empty;
    const StringArray names;
    std::unique_ptr<MidiInput> input;
    std::unique_ptr<MidiOutput> output;
    std::mutex inboxLock;
    std::deque<Bytes> inbox, outbox;
    std::array<int, 16> bank {};
    std::array<double, 11> refreshAt {};
    std::map<Address, var> pending;
    StringArray soundIncomplete;
    int activeRead = -1, allIndex = -1, lastProgress = 0, attempt = 0, selected = 0;
    int auditionPart = -1, auditionNote = 60;
    bool dumpRequested = false;
    double deadline = 0, identification = 0, nextPortCheck = 0;
};
} // namespace seqtrak
