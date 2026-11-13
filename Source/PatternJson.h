#pragma once

namespace seqtrak
{
inline var object (std::initializer_list<std::pair<Identifier, var>> properties)
{
    auto result = std::make_unique<DynamicObject>();
    for (const auto& [key, value] : properties) result->setProperty (key, value);
    return var (result.release());
}

inline var encodePattern (const Pattern& p)
{
    Array<var> tracks, samples;
    for (int t = 0; t < 11; ++t)
    {
        const auto index = size_t (t);
        const auto& s = p.sounds[index];
        Array<var> steps, voice, fx;
        for (int i = 0; i < 128; ++i)
        {
            const auto& step = p.sequence.step (t, i);
            if (step.on || step.note != 60 || step.velocity != 100 || step.probability != 100
                || step.micro != 0 || step.gate != 50)
                steps.add (Array<var> { i, step.on, step.note, step.velocity, step.probability, step.micro, step.gate });
        }
        for (const auto& [a, value] : s.voice) voice.add (Array<var> { a[0], a[1], a[2], value });
        for (const auto& [offset, value] : s.fx) fx.add (Array<var> { offset, value });
        tracks.add (object ({ {"length", p.sequence.lengths[index]}, {"mute", p.sequence.mix[index][0]},
            {"solo", p.sequence.mix[index][1]}, {"steps", steps}, {"number", s.number}, {"name", s.name},
            {"voice", voice}, {"effect", s.effect}, {"fx", fx} }));
    }
    for (int number : p.samples) samples.add (number);
    return object ({ {"name", p.name}, {"swing", p.sequence.swing}, {"tracks", tracks}, {"samples", samples} });
}

inline bool integer (const var& v, int min, int max)
{
    return v.isInt() && int (v) >= min && int (v) <= max;
}

inline bool decodePattern (const var& v, Pattern& p, const var& schema, const Hardware& hardware)
{
    const auto* tracks = v["tracks"].getArray();
    const auto* samples = v["samples"].getArray();
    if (!v["name"].isString() || !integer (v["swing"], 50, 75) || tracks == nullptr || tracks->size() != 11
        || samples == nullptr || samples->size() != 7) return false;
    p = {};
    p.name = v["name"].toString().substring (0, 64);
    p.sequence.swing = int (v["swing"]);
    for (int i = 0; i < 7; ++i)
    {
        if (!integer ((*samples)[i], 0, 392)) return false;
        p.samples[size_t (i)] = int ((*samples)[i]);
    }
    for (int t = 0; t < 11; ++t)
    {
        const auto index = size_t (t);
        const auto& row = (*tracks)[t];
        const auto* steps = row["steps"].getArray();
        const auto* voice = row["voice"].getArray();
        const auto* fx = row["fx"].getArray();
        if (!integer (row["length"], 1, 128) || !row["mute"].isBool() || !row["solo"].isBool()
            || !integer (row["number"], 0, t == 10 ? 392 : 2032) || !row["name"].isString()
            || !integer (row["effect"], -1, 16383) || steps == nullptr || steps->size() > 128
            || voice == nullptr || voice->size() > schema[t].getArray()->size() || fx == nullptr || fx->size() > 64)
            return false;
        p.sequence.lengths[index] = int (row["length"]);
        p.sequence.mix[index] = { bool (row["mute"]), bool (row["solo"]) };
        auto& sound = p.sounds[index];
        sound.number = int (row["number"]);
        sound.name = row["name"].toString().substring (0, 128);
        sound.effect = int (row["effect"]);
        for (const auto& step : *steps)
        {
            if (!step.isArray() || step.getArray()->size() != 7 || !integer (step[0], 0, 127)
                || !step[1].isBool() || !integer (step[2], 0, 127) || !integer (step[3], 1, 127)
                || !integer (step[4], 0, 100) || !integer (step[5], -50, 50) || !integer (step[6], 1, 100))
                return false;
            const int i = int (step[0]);
            p.sequence.grid[index][size_t (i / 16)][size_t (i % 16)] =
                { bool (step[1]), int (step[3]), int (step[2]), int (step[4]), int (step[5]), int (step[6]) };
        }
        for (const auto& entry : *voice)
        {
            if (!entry.isArray() || entry.getArray()->size() != 4 || !integer (entry[0], 0, 127)
                || !integer (entry[1], 0, 127) || !integer (entry[2], 0, 127)) return false;
            const Address a { int (entry[0]), int (entry[1]), int (entry[2]) };
            bool valid = false;
            for (const auto& def : *schema[t].getArray())
                if (address (def) == a && integer (entry[3], int (def["min"]), int (def["max"]))) valid = true;
            if (!valid) return false;
            sound.voice[a] = int (entry[3]);
        }
        for (const auto& def : *schema[t].getArray())
        {
            if (!def["id"].toString().endsWith (".wave")) continue;
            const auto a = address (def);
            const auto wave = sound.voice.find (a);
            const auto bank = sound.voice.find ({ a[0], a[1], 1 });
            if (wave != sound.voice.end() && bank != sound.voice.end() && bank->second == 1 && wave->second > 2048)
                return false;
        }
        const auto* parameters = hardware.effectFor (sound.effect)["params"].getArray();
        if (sound.effect >= 0 && parameters == nullptr) return false;
        for (const auto& entry : *fx)
        {
            if (parameters == nullptr || !entry.isArray() || entry.getArray()->size() != 2
                || !integer (entry[0], 0, 127)) return false;
            bool valid = false;
            for (const auto& def : *parameters)
                if (int (def["offset"]) == int (entry[0]) && integer (entry[1], int (def["min"]), int (def["max"]))) valid = true;
            if (!valid) return false;
            sound.fx[int (entry[0])] = int (entry[1]);
        }
    }
    return true;
}
} // namespace seqtrak
