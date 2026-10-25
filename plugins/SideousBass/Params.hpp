/*
 * Sideous Bass - shared parameter definitions used by both the DSP plugin
 * side and the UI, so ranges/units/labels can't drift between the two.
 *
 * Modulation routing lives entirely in the Mod Matrix (kParamModMatrixStart
 * .. kParamCount-1): 6 sources x 7 destinations = 42 depth parameters, see
 * dsp/BassVoice.hpp's ModSource/ModDest enums for the exact ordering (must
 * stay in sync with modMatrixParam() below). There is no other fixed
 * source->destination routing anywhere in this file.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sideous {

enum Params : uint32_t {
    kParamEngineMode = 0,
    kParamWaveform,
    kParamPulseWidth,
    kParamRingSyncRatio,
    kParamRingMix,
    kParamGrit,
    kParamEngineAttack,
    kParamEngineDecay,
    kParamEngineSustain,
    kParamEngineRelease,
    kParamEngineEnvCurve,
    kParamSubOctave,
    kParamSubLevel,
    kParamFilterMode,
    kParamFilterCutoff,
    kParamFilterResonance,
    kParamFilterDrive,
    kParamAmpAttack,
    kParamAmpDecay,
    kParamAmpSustain,
    kParamAmpRelease,
    kParamAmpCurve,
    kParamVelocitySensitivity,
    kParamFilterAttack,
    kParamFilterDecay,
    kParamFilterSustain,
    kParamFilterRelease,
    kParamFilterEnvCurve,
    kParamMasterVolume,
    kParamLfo1Waveform,
    kParamLfo1RateHz,
    kParamLfo1Sync,
    kParamLfo2Waveform,
    kParamLfo2RateHz,
    kParamLfo2Sync,
    kParamVoiceMode,
    kParamPortamentoTime,
    kParamPitchBendRange,
    kParamGlideMode,
    kParamModMatrixStart,
    kParamCount = kParamModMatrixStart + 6 * 7 // kModSourceCount * kModDestCount, see dsp/BassVoice.hpp
};

// index of the (source, dest) mod matrix cell - source/dest are the integer
// values of dsp/BassVoice.hpp's ModSource/ModDest enums. Kept here (rather
// than requiring every caller to know the row-major layout) so the matrix's
// physical parameter layout only needs to be right in one place.
inline constexpr uint32_t modMatrixParam(uint32_t source, uint32_t dest) noexcept
{
    return kParamModMatrixStart + source * 7 + dest;
}

enum class ParamShape { Linear, Logarithmic };

struct ParamInfo
{
    const char* name;
    const char* symbol;
    const char* unit;
    float min;
    float max;
    float def;
    ParamShape shape;
};

inline const ParamInfo& getParamInfo(uint32_t index) noexcept
{
    static constexpr ParamInfo table[kParamCount] = {
        { "Engine",           "engine",              "",   0.0f,     2.0f,     0.0f,   ParamShape::Linear },
        { "Waveform",         "waveform",            "",   0.0f,     2.0f,     0.0f,   ParamShape::Linear },
        { "Pulse Width",      "pulse_width",         "%",  1.0f,    99.0f,    50.0f,   ParamShape::Linear },
        { "Ratio",            "ring_sync_ratio",     "",   0.5f,     8.0f,     2.0f,   ParamShape::Logarithmic },
        { "Ring Mix",         "ring_mix",            "",   0.0f,     1.0f,     0.6f,   ParamShape::Linear },
        { "Grit",              "grit",               "",   0.0f,     1.0f,     0.5f,   ParamShape::Linear },
        { "Engine Attack",    "engine_attack",       "s",  0.001f,   5.0f,     0.003f, ParamShape::Logarithmic },
        { "Engine Decay",     "engine_decay",        "s",  0.001f,   5.0f,     0.15f,  ParamShape::Logarithmic },
        { "Engine Sustain",   "engine_sustain",      "",   0.0f,     1.0f,     0.2f,   ParamShape::Linear },
        { "Engine Release",   "engine_release",      "s",  0.001f,   8.0f,     0.12f,  ParamShape::Logarithmic },
        { "Engine Env Curve", "engine_env_curve",    "",   0.0f,     1.0f,     0.35f,  ParamShape::Linear },
        { "Sub Octave",       "sub_octave",          "",  -2.0f,     2.0f,    -1.0f,   ParamShape::Linear },
        { "Sub Level",        "sub_level",           "",   0.0f,     1.0f,     0.3f,   ParamShape::Linear },
        { "Filter Mode",      "filter_mode",         "",   0.0f,     5.0f,     0.0f,   ParamShape::Linear },
        { "Filter Cutoff",    "filter_cutoff",       "Hz", 20.0f, 20000.0f, 1200.0f,   ParamShape::Logarithmic },
        { "Filter Resonance", "filter_resonance",    "",   0.0f,     1.0f,     0.3f,   ParamShape::Linear },
        { "Filter Drive",     "filter_drive",        "",   0.0f,     1.0f,     0.1f,   ParamShape::Linear },
        { "Amp Attack",       "amp_attack",          "s",  0.001f,   5.0f,     0.003f, ParamShape::Logarithmic },
        { "Amp Decay",        "amp_decay",           "s",  0.001f,   5.0f,     0.15f,  ParamShape::Logarithmic },
        { "Amp Sustain",      "amp_sustain",         "",   0.0f,     1.0f,     0.6f,   ParamShape::Linear },
        { "Amp Release",      "amp_release",         "s",  0.001f,   8.0f,     0.15f,  ParamShape::Logarithmic },
        { "Amp Env Curve",    "amp_curve",           "",   0.0f,     1.0f,     0.35f,  ParamShape::Linear },
        { "Velocity Sens",    "velocity_sens",       "",   0.0f,     1.0f,     0.7f,   ParamShape::Linear },
        { "Filter Attack",    "filter_attack",       "s",  0.001f,   5.0f,     0.003f, ParamShape::Logarithmic },
        { "Filter Decay",     "filter_decay",        "s",  0.001f,   5.0f,     0.2f,   ParamShape::Logarithmic },
        { "Filter Sustain",   "filter_sustain",      "",   0.0f,     1.0f,     0.0f,   ParamShape::Linear },
        { "Filter Release",   "filter_release",      "s",  0.001f,   8.0f,     0.15f,  ParamShape::Logarithmic },
        { "Filter Env Curve", "filter_env_curve",    "",   0.0f,     1.0f,     0.35f,  ParamShape::Linear },
        { "Master Volume",    "master_volume",       "",   0.0f,     1.0f,     0.8f,   ParamShape::Linear },
        { "LFO1 Waveform",    "lfo1_waveform",       "",   0.0f,     2.0f,     0.0f,   ParamShape::Linear },
        { "LFO1 Rate",        "lfo1_rate_hz",        "Hz", 0.02f,   20.0f,     2.0f,   ParamShape::Logarithmic },
        { "LFO1 Sync",        "lfo1_sync",           "",   0.0f,    14.0f,     0.0f,   ParamShape::Linear },
        { "LFO2 Waveform",    "lfo2_waveform",       "",   0.0f,     2.0f,     0.0f,   ParamShape::Linear },
        { "LFO2 Rate",        "lfo2_rate_hz",        "Hz", 0.02f,   20.0f,     3.0f,   ParamShape::Logarithmic },
        { "LFO2 Sync",        "lfo2_sync",           "",   0.0f,    14.0f,     0.0f,   ParamShape::Linear },
        { "Voice Mode",       "voice_mode",          "",   0.0f,     1.0f,     1.0f,   ParamShape::Linear },
        { "Portamento",       "portamento",          "s",  0.0f,     3.0f,     0.05f,  ParamShape::Linear },
        { "Pitch Bend Range", "pitch_bend_range",    "st", 1.0f,    24.0f,     2.0f,   ParamShape::Linear },
        { "Glide Mode",       "glide_mode",          "",   0.0f,     1.0f,     0.0f,   ParamShape::Linear },

        // --- mod matrix: 6 sources x 7 destinations, row-major (source outer,
        // dest inner), see modMatrixParam() above. All default to 0 = no
        // connection - this is the entire modulation routing model.
        { "LFO1>Pitch",       "mod_lfo1_pitch",      "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "LFO1>Cutoff",      "mod_lfo1_cutoff",     "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "LFO1>Ratio",       "mod_lfo1_ratio",      "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "LFO1>Grit",        "mod_lfo1_grit",       "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "LFO1>RingMix",     "mod_lfo1_ringmix",    "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "LFO1>PW",          "mod_lfo1_pw",         "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "LFO1>Amp",         "mod_lfo1_amp",        "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },

        { "LFO2>Pitch",       "mod_lfo2_pitch",      "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "LFO2>Cutoff",      "mod_lfo2_cutoff",     "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "LFO2>Ratio",       "mod_lfo2_ratio",      "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "LFO2>Grit",        "mod_lfo2_grit",       "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "LFO2>RingMix",     "mod_lfo2_ringmix",    "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "LFO2>PW",          "mod_lfo2_pw",         "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "LFO2>Amp",         "mod_lfo2_amp",        "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },

        { "AmpEnv>Pitch",     "mod_ampenv_pitch",    "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "AmpEnv>Cutoff",    "mod_ampenv_cutoff",   "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "AmpEnv>Ratio",     "mod_ampenv_ratio",    "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "AmpEnv>Grit",      "mod_ampenv_grit",     "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "AmpEnv>RingMix",   "mod_ampenv_ringmix",  "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "AmpEnv>PW",        "mod_ampenv_pw",       "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "AmpEnv>Amp",       "mod_ampenv_amp",      "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },

        { "FilterEnv>Pitch",  "mod_filterenv_pitch", "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "FilterEnv>Cutoff", "mod_filterenv_cutoff","",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "FilterEnv>Ratio",  "mod_filterenv_ratio", "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "FilterEnv>Grit",   "mod_filterenv_grit",  "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "FilterEnv>RingMix","mod_filterenv_ringmix","", -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "FilterEnv>PW",     "mod_filterenv_pw",    "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "FilterEnv>Amp",    "mod_filterenv_amp",   "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },

        { "EngineEnv>Pitch",  "mod_engineenv_pitch", "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "EngineEnv>Cutoff", "mod_engineenv_cutoff","",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "EngineEnv>Ratio",  "mod_engineenv_ratio", "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "EngineEnv>Grit",   "mod_engineenv_grit",  "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "EngineEnv>RingMix","mod_engineenv_ringmix","", -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "EngineEnv>PW",     "mod_engineenv_pw",    "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "EngineEnv>Amp",    "mod_engineenv_amp",   "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },

        { "ModWheel>Pitch",   "mod_modwheel_pitch",  "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "ModWheel>Cutoff",  "mod_modwheel_cutoff", "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "ModWheel>Ratio",   "mod_modwheel_ratio",  "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "ModWheel>Grit",    "mod_modwheel_grit",   "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "ModWheel>RingMix", "mod_modwheel_ringmix","",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "ModWheel>PW",      "mod_modwheel_pw",     "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
        { "ModWheel>Amp",     "mod_modwheel_amp",    "",  -1.0f, 1.0f, 0.0f, ParamShape::Linear },
    };
    return table[index];
}

// Hosts automate a parameter's range linearly (VST3 normalisation, CLAP
// plain values; DPF passes the logarithmic hint on to LV2 only), which crams
// most of a 20 Hz..20 kHz or 1 ms..5 s range into the first few percent of an
// automation lane. So every Logarithmic parameter is exposed to the host as
// a 0..100 % position along its log curve (for a cutoff, ~10 % per octave).
// The DSP, the UI and preset files keep working in real units: the plugin
// and the UI convert only where values cross to and from the host.
inline bool isHostPosition(uint32_t index) noexcept
{
    return index < kParamCount && getParamInfo(index).shape == ParamShape::Logarithmic;
}

inline float paramToHost(uint32_t index, float value) noexcept
{
    if (!isHostPosition(index))
        return value;
    const ParamInfo& info = getParamInfo(index);
    const float lo = std::max(info.min, 1e-6f);
    const float v = std::clamp(value, lo, info.max);
    return 100.0f * std::log(v / lo) / std::log(info.max / lo);
}

inline float paramFromHost(uint32_t index, float value) noexcept
{
    if (!isHostPosition(index))
        return value;
    const ParamInfo& info = getParamInfo(index);
    const float lo = std::max(info.min, 1e-6f);
    return lo * std::pow(info.max / lo, std::clamp(value, 0.0f, 100.0f) * 0.01f);
}

} // namespace sideous
