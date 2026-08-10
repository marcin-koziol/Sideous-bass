/*
 * Sideous Bass - a handful of factory example presets, seeded into the
 * user's preset directory (see ui/PresetStore.hpp) the first time the UI
 * ever runs with an empty preset library. Compiled in (rather than shipped
 * as separate data files) so they exist identically across every build/
 * install of the plugin regardless of platform or plugin format - no
 * bundle-path discovery needed, they just get written out as ordinary,
 * user-editable ".sbpreset" files on first run and behave exactly like any
 * preset the user saves themselves from that point on.
 */

#pragma once

#include "PresetStore.hpp"

#include <cstring>
#include <utility>
#include <vector>

namespace sideous {
namespace ui {

struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> overrides; // symbol -> value; everything else stays at that param's Params.hpp default
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        // Ring engine, sub-heavy, low ladder cutoff with a slow filter-env
        // growl - the "plain" starting point for the classic metallic bass
        { "Deep Sub Growl", {
            { "ring_sync_ratio", 1.5f }, { "ring_mix", 0.45f },
            { "sub_level", 0.6f },
            { "filter_mode", 5.0f }, { "filter_cutoff", 320.0f }, { "filter_resonance", 0.4f }, { "filter_drive", 0.25f },
            { "amp_attack", 0.004f }, { "amp_decay", 0.25f }, { "amp_sustain", 0.75f }, { "amp_release", 0.2f },
            { "filter_attack", 0.004f }, { "filter_decay", 0.35f }, { "filter_sustain", 0.15f }, { "filter_release", 0.2f },
            { "mod_filterenv_cutoff", 0.55f },
            { "portamento", 0.03f },
        }},
        // Sync engine with Engine Env -> Ratio patched: the classic "swept
        // sync" bright buzzy attack settling into the sustain ratio
        { "Sync Scream Lead", {
            { "engine", 1.0f },
            { "ring_sync_ratio", 2.2f },
            { "engine_attack", 0.002f }, { "engine_decay", 0.22f }, { "engine_sustain", 0.1f }, { "engine_release", 0.12f },
            { "mod_engineenv_ratio", 0.85f },
            { "sub_level", 0.15f },
            { "filter_mode", 1.0f }, { "filter_cutoff", 900.0f }, { "filter_resonance", 0.5f }, { "filter_drive", 0.3f },
            { "amp_attack", 0.002f }, { "amp_decay", 0.18f }, { "amp_sustain", 0.55f }, { "amp_release", 0.15f },
        }},
        // Combined (SID quirk) engine, high Grit, short punchy pluck
        { "SID Digi Crunch", {
            { "engine", 2.0f },
            { "grit", 0.8f }, { "pulse_width", 35.0f },
            { "sub_level", 0.2f },
            { "filter_mode", 0.0f }, { "filter_cutoff", 1800.0f }, { "filter_resonance", 0.2f }, { "filter_drive", 0.15f },
            { "amp_attack", 0.001f }, { "amp_decay", 0.09f }, { "amp_sustain", 0.2f }, { "amp_release", 0.08f },
            { "filter_attack", 0.001f }, { "filter_decay", 0.1f }, { "filter_sustain", 0.0f }, { "filter_release", 0.08f },
            { "mod_filterenv_cutoff", 0.4f },
        }},
        // Ring engine with LFO1 tempo-synced to 1/8 patched into Cutoff -
        // a rhythmic wobble
        { "Wobble Bass", {
            { "ring_sync_ratio", 1.25f }, { "ring_mix", 0.7f },
            { "sub_level", 0.35f },
            { "filter_mode", 5.0f }, { "filter_cutoff", 600.0f }, { "filter_resonance", 0.55f }, { "filter_drive", 0.2f },
            { "lfo1_sync", 9.0f }, // 1/8 straight
            { "mod_lfo1_cutoff", 0.75f },
            { "amp_sustain", 0.85f },
        }},
        // Ring engine, portamento-forward classic mono growl with a filter sweep
        { "Mono Growl Glide", {
            { "ring_sync_ratio", 1.8f }, { "ring_mix", 0.35f },
            { "sub_level", 0.45f },
            { "filter_mode", 5.0f }, { "filter_cutoff", 500.0f }, { "filter_resonance", 0.35f }, { "filter_drive", 0.3f },
            { "filter_attack", 0.005f }, { "filter_decay", 0.3f }, { "filter_sustain", 0.2f }, { "filter_release", 0.25f },
            { "mod_filterenv_cutoff", 0.5f },
            { "amp_decay", 0.2f }, { "amp_sustain", 0.8f }, { "amp_release", 0.25f },
            { "portamento", 0.09f },
        }},
        // 8-voice Poly mode, short-ish envelope - stacked bass chords/fifths
        // instead of the usual single-note bassline
        { "Chord Stab", {
            { "voice_mode", 0.0f }, // Poly
            { "ring_sync_ratio", 1.5f }, { "ring_mix", 0.4f },
            { "sub_level", 0.25f },
            { "filter_mode", 1.0f }, { "filter_cutoff", 1100.0f }, { "filter_resonance", 0.25f }, { "filter_drive", 0.1f },
            { "amp_attack", 0.002f }, { "amp_decay", 0.15f }, { "amp_sustain", 0.4f }, { "amp_release", 0.12f },
        }},
        // Combined engine, multiple simultaneous Patch Bay connections at
        // once (LFO1->Cutoff, LFO2->Grit, Mod Wheel->Ring Mix) - a deliberate
        // "look how many cables you can run at once" showcase patch
        { "Talking Wobble", {
            { "engine", 2.0f },
            { "grit", 0.55f }, { "pulse_width", 40.0f },
            { "sub_level", 0.3f },
            { "filter_mode", 5.0f }, { "filter_cutoff", 700.0f }, { "filter_resonance", 0.4f }, { "filter_drive", 0.2f },
            { "lfo1_sync", 9.0f }, { "mod_lfo1_cutoff", 0.5f },     // 1/8 wobble
            { "lfo2_rate_hz", 0.6f }, { "mod_lfo2_grit", 0.45f },   // slow grit drift
            { "mod_modwheel_ringmix", 0.6f },                       // performance control on top
            { "amp_sustain", 0.8f },
        }},
        // Triangle wave, Ring engine turned almost fully dry (Ring Mix low),
        // slow attack, warm lowpass - about as close to a clean upright/DI
        // bass tone as this buzzy-by-design instrument gets
        { "Upright Thump", {
            { "waveform", 2.0f }, // Triangle
            { "ring_sync_ratio", 1.0f }, { "ring_mix", 0.1f },
            { "sub_octave", -1.0f }, { "sub_level", 0.5f },
            { "filter_mode", 0.0f }, { "filter_cutoff", 750.0f }, { "filter_resonance", 0.1f }, { "filter_drive", 0.0f },
            { "amp_attack", 0.015f }, { "amp_decay", 0.2f }, { "amp_sustain", 0.7f }, { "amp_release", 0.3f },
        }},
        // Sync engine into a ladder filter pushed toward self-oscillation,
        // heavy drive - the aggressive, near-growling end of the range
        { "Self-Osc Growl", {
            { "engine", 1.0f },
            { "ring_sync_ratio", 3.5f },
            { "sub_level", 0.2f },
            { "filter_mode", 5.0f }, { "filter_cutoff", 350.0f }, { "filter_resonance", 0.85f }, { "filter_drive", 0.6f },
            { "filter_attack", 0.002f }, { "filter_decay", 0.4f }, { "filter_sustain", 0.3f }, { "filter_release", 0.3f },
            { "mod_filterenv_cutoff", 0.6f },
            { "amp_attack", 0.002f }, { "amp_decay", 0.2f }, { "amp_sustain", 0.7f }, { "amp_release", 0.2f },
        }},
        // Ring engine, LFO2 patched into Pitch at a small depth - a
        // slow, subtle vibrato instead of anything patched to Cutoff/Ratio
        { "Vibrato Sub", {
            { "ring_sync_ratio", 1.3f }, { "ring_mix", 0.3f },
            { "sub_level", 0.55f },
            { "filter_mode", 5.0f }, { "filter_cutoff", 450.0f }, { "filter_resonance", 0.2f }, { "filter_drive", 0.1f },
            { "lfo2_rate_hz", 4.5f }, { "mod_lfo2_pitch", 0.06f },
            { "amp_attack", 0.01f }, { "amp_decay", 0.2f }, { "amp_sustain", 0.8f }, { "amp_release", 0.3f },
        }},
        // Mod Wheel patched to Cutoff at full depth - a performance patch
        // built specifically to be played with the wheel during a take
        { "Mod Wheel Filter Sweep", {
            { "ring_sync_ratio", 1.6f }, { "ring_mix", 0.5f },
            { "sub_level", 0.3f },
            { "filter_mode", 1.0f }, { "filter_cutoff", 250.0f }, { "filter_resonance", 0.5f }, { "filter_drive", 0.2f },
            { "mod_modwheel_cutoff", 1.0f },
            { "amp_sustain", 0.85f },
        }},
    };
    return presets;
}

// writes every factory preset to disk via the normal savePreset() path (so
// they're indistinguishable from user-saved presets from that point on) -
// call once, only when the preset library is empty (first run).
inline void seedFactoryPresets()
{
    for (const FactoryPreset& fp : factoryPresets())
    {
        float values[kParamCount];
        for (uint32_t i = 0; i < kParamCount; ++i)
            values[i] = getParamInfo(i).def;

        for (const auto& kv : fp.overrides)
        {
            for (uint32_t i = 0; i < kParamCount; ++i)
            {
                if (std::strcmp(getParamInfo(i).symbol, kv.first) == 0)
                {
                    values[i] = kv.second;
                    break;
                }
            }
        }

        savePreset(fp.name, values);
    }
}

} // namespace ui
} // namespace sideous
