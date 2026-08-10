/*
 * Sideous Bass - a single synth voice: one of three selectable oscillator
 * engines (Ring / Sync / Combined) plus a sub-oscillator -> amp ADSR, in
 * parallel with a filter, driven by a small modular-style mod matrix: six
 * sources (LFO1, LFO2, Amp Env, Filter Env, Engine Env, Mod Wheel) each
 * freely patchable to any/all of seven destinations (Pitch, Cutoff, Ratio,
 * Grit, Ring Mix, Pulse Width, Amplitude) via a per-(source,destination)
 * depth. Amp Env additionally always drives the voice's own VCA - that one
 * connection is hardwired, not patchable, or the instrument would default
 * to silence. Modeled on sideous's Voice.hpp - same overall shape, so the
 * voice-pool/stealing code from that project ports over with minimal
 * changes.
 */

#pragma once

#include <cmath>
#include <algorithm>

#include "Oscillator.hpp"
#include "RingSyncOscillator.hpp"
#include "CombinedOscillator.hpp"
#include "ADSR.hpp"
#include "Filter.hpp"
#include "LadderFilter.hpp"
#include "LFO.hpp"

namespace sideous {

enum class EngineMode { Ring = 0, Sync, Combined };

// modulation matrix: sourceVal[source] * modDepth[source][dest], summed
// across sources, offsets each destination's base value. Order matters -
// it's the array index into VoiceParams::modDepth and BassVoice's internal
// per-sample sourceVal[]/modSum[] arrays, and must match Params.hpp's
// kParamModMatrix layout and UIPainter.hpp's jack ordering.
enum class ModSource { Lfo1 = 0, Lfo2, AmpEnv, FilterEnv, EngineEnv, ModWheel };
static constexpr int kModSourceCount = 6;

enum class ModDest { Pitch = 0, Cutoff, Ratio, Grit, RingMix, PulseWidth, Amplitude };
static constexpr int kModDestCount = 7;

struct VoiceParams
{
    // every default below must match its counterpart in Params.hpp exactly -
    // that table is the single source of truth for "what a fresh instance
    // (or the Patch Bay's INIT slot) actually sounds like"; these are the
    // struct's own compiled-in starting values, which DPF does *not*
    // auto-sync from Params.hpp's declared ranges.def on its own.
    EngineMode engineMode = EngineMode::Ring;
    Waveform waveform = Waveform::Saw;
    float pulseWidth = 0.5f;         // base value; Mod Matrix -> Pulse Width offsets it per-sample
    float ringSyncRatio = 2.0f;      // base value; Mod Matrix -> Ratio offsets it per-sample (shared by Ring and Sync)
    float ringMix = 0.6f;            // base value; Mod Matrix -> Ring Mix offsets it per-sample (Ring only)
    float grit = 0.5f;               // base value; Mod Matrix -> Grit offsets it per-sample (Combined only)

    float subOctave = -1.0f;        // -2, -1, +1, or +2
    float subLevel = 0.3f;          // 0..1, mixed in pre-filter

    FilterMode filterMode = FilterMode::LP12;
    float filterCutoff = 1200.0f;   // Hz, base cutoff; Mod Matrix -> Cutoff offsets it per-sample
    float filterResonance = 0.3f;   // 0..1
    float filterDrive = 0.1f;       // 0..1

    float ampAttack = 0.003f, ampDecay = 0.15f, ampSustain = 0.6f, ampRelease = 0.15f;
    float ampCurve = 0.35f;         // 0..1, 0 = linear, 1 = analog-style
    float velocitySensitivity = 0.7f; // 0 = velocity ignored, 1 = full response

    float filterAttack = 0.003f, filterDecay = 0.2f, filterSustain = 0.0f, filterRelease = 0.15f;
    float filterCurve = 0.35f;      // 0..1, 0 = linear, 1 = analog-style

    // envelope generator only - what it modulates (if anything) is decided
    // entirely by the mod matrix (Engine Env is a source row, see below)
    float engineAttack = 0.003f, engineDecay = 0.15f, engineSustain = 0.2f, engineRelease = 0.12f;
    float engineCurve = 0.35f;      // 0..1, 0 = linear, 1 = analog-style

    LfoWaveform lfo1Waveform = LfoWaveform::Sine;
    LfoWaveform lfo2Waveform = LfoWaveform::Sine;
    // LFO frequencies are set separately per-block from tempo, see
    // setLfo1Frequency()/setLfo2Frequency() below - not part of VoiceParams

    float portamentoTime = 0.05f;   // seconds; 0 = off, snaps to the new pitch

    // modDepth[source][dest]: -1..1, 0 = no connection. This is the entire
    // routing model - there is no other fixed source->destination wiring
    // anywhere else in this struct (besides Amp Env's hardwired VCA role,
    // handled internally by BassVoice, not through this matrix). The one
    // exception is modDepth[AmpEnv][Amplitude] itself: the parameter slot
    // exists (so preset files/automation referencing it don't break
    // anything), but BassVoice::process() deliberately ignores it - see the
    // comment there for why.
    float modDepth[kModSourceCount][kModDestCount] = {};

    // pitch bend range and the live mod wheel/pitch bend values themselves
    // are NOT here - they're continuous controllers pushed every block via
    // setModWheel()/setPitchBend(), see below.
};

class BassVoice
{
public:
    // must match Params.hpp's kParamFilterCutoff range
    static constexpr float kCutoffMin = 20.0f;
    static constexpr float kCutoffMax = 20000.0f;
    // must match Params.hpp's kParamRingSyncRatio range / RingSyncOscillator's own clamp
    static constexpr float kRatioMin = 0.5f;
    static constexpr float kRatioMax = 8.0f;
    // semitone swing at Mod Matrix -> Pitch modSum = 1 (a single source at
    // full depth); two octaves gives plenty of headroom for both subtle
    // vibrato (small depth) and dramatic siren sweeps (depth near 1)
    static constexpr float kPitchModMaxSemitones = 24.0f;

    void setSampleRate(double sampleRate) noexcept
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        fRingSync.setSampleRate(sampleRate);
        fCombined.setSampleRate(sampleRate);
        fSubOsc.setSampleRate(sampleRate);
        fAmpEnv.setSampleRate(sampleRate);
        fFilterEnv.setSampleRate(sampleRate);
        fEngineEnv.setSampleRate(sampleRate);
        fFilterA.setSampleRate(sampleRate);
        fFilterB.setSampleRate(sampleRate);
        fLadder.setSampleRate(sampleRate);
        fLfo1.setSampleRate(sampleRate);
        fLfo2.setSampleRate(sampleRate);
    }

    // retriggerEnvelope=false gives legato behavior: the pitch still moves
    // (gliding via portamento if set) but the amp/filter/engine envelopes
    // and both LFOs' phase are left running rather than restarting - used
    // for mono-mode note-stack fallback and overlapping ("legato") playing.
    //
    // glideFromFreq is a 3-way sentinel controlling where a glide starts from:
    //   < 0  "auto"           - glide only if this voice was already active
    //                           (uses its current pitch as the source); this
    //                           is the right choice when the caller has
    //                           already confirmed a genuine overlap
    //   == 0 "force no glide" - always snap straight to the target pitch,
    //                           even if the voice is still audibly ringing
    //                           out a release (e.g. a fresh, non-overlapping
    //                           staccato attack in Legato glide mode)
    //   > 0  "force glide"    - always glide, starting from this frequency,
    //                           regardless of whether the voice was active
    //                           (Always glide mode, or poly's "glide from
    //                           whatever note last played" behavior)
    void noteOn(int note, float velocity, bool retriggerEnvelope = true, float glideFromFreq = -1.0f) noexcept
    {
        fNote = note;
        fVelocity = velocity;
        const float targetFreq = noteToHz(note);
        const float sourceFreq = glideFromFreq > 0.0f ? glideFromFreq
                                : glideFromFreq == 0.0f ? -1.0f
                                : (fActive ? fCurrentFreq : -1.0f);

        if (sourceFreq > 0.0f && fPortamentoTime > 0.0005f)
        {
            fGlideFromLog = std::log2(sourceFreq);
            fGlideToLog = std::log2(targetFreq);
            fGlidePos = 0.0f;
            fGlideIncrement = (float)(1.0 / ((double)fPortamentoTime * fSampleRate));
        }
        else
        {
            fCurrentFreq = targetFreq;
            fGlidePos = 1.0f;
        }

        if (retriggerEnvelope)
        {
            fAmpEnv.noteOn();
            fFilterEnv.noteOn();
            fEngineEnv.noteOn();
            fLfo1.reset();
            fLfo2.reset();
            fRingSync.resetPhase();
            fCombined.resetPhase();
            fSubOsc.resetPhase();
        }
        fActive = true;
    }

    void noteOff() noexcept
    {
        fAmpEnv.noteOff();
        fFilterEnv.noteOff();
        fEngineEnv.noteOff();
    }

    // hard cut, used for voice stealing
    void kill() noexcept
    {
        fActive = false;
    }

    bool isActive() const noexcept { return fActive && !fAmpEnv.isIdle(); }
    bool isReleasing() const noexcept { return fAmpEnv.getStage() == ADSR::Stage::Release; }
    int getNote() const noexcept { return fNote; }

    // set every audio block from the host tempo (or the free-rate knobs);
    // kept separate from applyParams() since they need to track tempo
    // changes without re-touching every other per-note setting each block
    void setLfo1Frequency(float hz) noexcept { fLfo1.setFrequency(hz); }
    void setLfo2Frequency(float hz) noexcept { fLfo2.setFrequency(hz); }

    // continuous MIDI controllers, also pushed every block regardless of
    // whether a note is currently sounding (so they're already in effect
    // the instant the next note starts)
    void setModWheel(float value01) noexcept { fModWheel = value01; }
    void setPitchBend(float semitones) noexcept { fPitchBendSemitones = semitones; }

    void applyParams(const VoiceParams& p) noexcept
    {
        fEngineMode = p.engineMode;
        fRingSync.setMode(p.engineMode == EngineMode::Sync ? RingSyncMode::Sync : RingSyncMode::Ring);
        fRingSync.setWaveform(p.waveform);
        fSubOsc.setWaveform(p.waveform);
        fBasePulseWidth = p.pulseWidth; // actual pulse width is computed per-sample in process(), see below
        fBaseRatio = p.ringSyncRatio;   // actual ratio is computed per-sample in process(), see below
        fBaseRingMix = p.ringMix;       // actual ring mix is computed per-sample in process(), see below
        fBaseGrit = p.grit;             // actual grit is computed per-sample in process(), see below
        fSubOctave = p.subOctave;
        fSubLevel = p.subLevel;

        fFilterMode = p.filterMode;
        const FilterType stageType = p.filterMode == FilterMode::HP12 || p.filterMode == FilterMode::HP24
                                    ? FilterType::Highpass
                                    : p.filterMode == FilterMode::BP12
                                    ? FilterType::Bandpass
                                    : FilterType::Lowpass;
        fFilterA.setType(stageType);
        fFilterB.setType(stageType);
        fFilterA.setResonance(p.filterResonance);
        // for the 24dB modes, only the first stage carries the resonant peak;
        // giving both stages the same high resonance compounds their gain
        // quadratically and blows well past sane levels (peaks of 400+ measured)
        fFilterB.setResonance(0.0f);
        fFilterA.setDrive(p.filterDrive);
        fLadder.setResonance(p.filterResonance);
        fLadder.setDrive(p.filterDrive);

        fAmpEnv.setAttack(p.ampAttack);
        fAmpEnv.setDecay(p.ampDecay);
        fAmpEnv.setSustain(p.ampSustain);
        fAmpEnv.setRelease(p.ampRelease);
        fAmpEnv.setCurve(p.ampCurve);
        fVelocitySensitivity = p.velocitySensitivity;

        fFilterEnv.setAttack(p.filterAttack);
        fFilterEnv.setDecay(p.filterDecay);
        fFilterEnv.setSustain(p.filterSustain);
        fFilterEnv.setRelease(p.filterRelease);
        fFilterEnv.setCurve(p.filterCurve);

        fBaseCutoff = p.filterCutoff;

        fEngineEnv.setAttack(p.engineAttack);
        fEngineEnv.setDecay(p.engineDecay);
        fEngineEnv.setSustain(p.engineSustain);
        fEngineEnv.setRelease(p.engineRelease);
        fEngineEnv.setCurve(p.engineCurve);

        fLfo1.setWaveform(p.lfo1Waveform);
        fLfo2.setWaveform(p.lfo2Waveform);

        fPortamentoTime = p.portamentoTime;

        for (int s = 0; s < kModSourceCount; ++s)
            for (int d = 0; d < kModDestCount; ++d)
                fModDepth[s][d] = p.modDepth[s][d];
    }

    float process() noexcept
    {
        if (!fActive)
            return 0.0f;

        if (fGlidePos < 1.0f)
        {
            fGlidePos += fGlideIncrement;
            if (fGlidePos >= 1.0f)
            {
                fGlidePos = 1.0f;
                fCurrentFreq = std::exp2(fGlideToLog);
            }
            else
            {
                fCurrentFreq = std::exp2(fGlideFromLog + (fGlideToLog - fGlideFromLog) * fGlidePos);
            }
        }

        // every modulation source is read exactly once per sample, up
        // front, so the mod matrix below can freely reuse each value
        // across as many destinations as the user has patched it to.
        const float lfo1 = fLfo1.process();
        const float lfo2 = fLfo2.process();
        const float ampLevel = fAmpEnv.process();       // also the voice's hardwired VCA gain, see return below
        const float filterEnvLevel = fFilterEnv.process();
        const float engineEnvLevel = fEngineEnv.process();

        if (fAmpEnv.isIdle())
        {
            fActive = false;
            return 0.0f;
        }

        float sourceVal[kModSourceCount];
        sourceVal[(int)ModSource::Lfo1] = lfo1;
        sourceVal[(int)ModSource::Lfo2] = lfo2;
        sourceVal[(int)ModSource::AmpEnv] = ampLevel;
        sourceVal[(int)ModSource::FilterEnv] = filterEnvLevel;
        sourceVal[(int)ModSource::EngineEnv] = engineEnvLevel;
        sourceVal[(int)ModSource::ModWheel] = fModWheel;

        float modSum[kModDestCount] = {};
        for (int s = 0; s < kModSourceCount; ++s)
        {
            for (int d = 0; d < kModDestCount; ++d)
            {
                // Amp Env -> Amplitude is skipped: Amp Env already
                // unconditionally drives the VCA (see the return statement
                // below), so this one cell would double-apply the same
                // envelope on top of itself rather than add a new
                // modulation - inert by design, not just left at 0.
                if (s == (int)ModSource::AmpEnv && d == (int)ModDest::Amplitude)
                    continue;
                modSum[d] += fModDepth[s][d] * sourceVal[s];
            }
        }

        const float pitchSemitones = modSum[(int)ModDest::Pitch] * kPitchModMaxSemitones + fPitchBendSemitones;
        const float pitchRatio = std::exp2(pitchSemitones / 12.0f);

        // Ratio/Grit/Ring Mix/Pulse Width must all be set *before*
        // setFrequency() below: RingSyncOscillator derives the modulator's
        // frequency from Ratio at the moment setFrequency() is called, so
        // setting any of these after generating this sample would apply
        // the modulation one sample late.
        float rt = ratioToNormalized(fBaseRatio) + modSum[(int)ModDest::Ratio];
        rt = rt < 0.0f ? 0.0f : (rt > 1.0f ? 1.0f : rt);
        fRingSync.setRatio(normalizedToRatio(rt));

        const float grit = clamp01(fBaseGrit + modSum[(int)ModDest::Grit]);
        fCombined.setGrit(grit);

        const float ringMix = clamp01(fBaseRingMix + modSum[(int)ModDest::RingMix]);
        fRingSync.setMix(ringMix);

        float pwt = (fBasePulseWidth - 0.01f) / 0.98f + modSum[(int)ModDest::PulseWidth];
        pwt = clamp01(pwt);
        const float pulseWidth = 0.01f + pwt * 0.98f;
        fRingSync.setPulseWidth(pulseWidth);
        fCombined.setPulseWidth(pulseWidth);

        const float voicedFreq = fCurrentFreq * pitchRatio;
        fRingSync.setFrequency(voicedFreq);
        fCombined.setFrequency(voicedFreq);
        fSubOsc.setFrequency(fCurrentFreq * std::exp2(fSubOctave) * pitchRatio);

        const float engineOut = fEngineMode == EngineMode::Combined ? fCombined.process() : fRingSync.process();
        const float osc = engineOut + fSubOsc.process() * fSubLevel;

        // base cutoff + mod matrix combine in normalized log-frequency
        // space, so at modSum = 1 a single full-depth source can sweep the
        // *entire* cutoff range regardless of where the base Cutoff knob sits
        float ct = cutoffToNormalized(fBaseCutoff) + modSum[(int)ModDest::Cutoff];
        ct = ct < 0.0f ? 0.0f : (ct > 1.0f ? 1.0f : ct);
        const float cutoff = normalizedToCutoff(ct);

        float filtered;
        switch (fFilterMode)
        {
        case FilterMode::LP24:
        case FilterMode::HP24:
            filtered = fFilterB.process(fFilterA.process(osc, cutoff), cutoff);
            break;
        case FilterMode::Ladder24:
            filtered = fLadder.process(osc, cutoff);
            break;
        case FilterMode::LP12:
        case FilterMode::HP12:
        case FilterMode::BP12:
        default:
            filtered = fFilterA.process(osc, cutoff);
            break;
        }

        // amount=1 alone -> up to +6dB-ish boost, amount=-1 -> full mute
        const float ampMod = std::max(0.0f, std::min(2.0f, 1.0f + modSum[(int)ModDest::Amplitude]));

        const float velocityGain = 1.0f - fVelocitySensitivity * (1.0f - fVelocity);
        return filtered * ampLevel * velocityGain * ampMod;
    }

private:
    static float clamp01(float t) noexcept
    {
        return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    }

    static float noteToHz(int note) noexcept
    {
        return 440.0f * std::exp2(((float)note - 69.0f) / 12.0f);
    }

    static float cutoffToNormalized(float hz) noexcept
    {
        return std::log(hz / kCutoffMin) / std::log(kCutoffMax / kCutoffMin);
    }

    static float normalizedToCutoff(float t) noexcept
    {
        return kCutoffMin * std::pow(kCutoffMax / kCutoffMin, t);
    }

    static float ratioToNormalized(float ratio) noexcept
    {
        return std::log(ratio / kRatioMin) / std::log(kRatioMax / kRatioMin);
    }

    static float normalizedToRatio(float t) noexcept
    {
        return kRatioMin * std::pow(kRatioMax / kRatioMin, t);
    }

    RingSyncOscillator fRingSync;
    CombinedOscillator fCombined;
    Oscillator fSubOsc;
    ADSR fAmpEnv;
    ADSR fFilterEnv;
    ADSR fEngineEnv;
    Filter fFilterA;
    Filter fFilterB;      // only used for the 24dB cascaded modes
    LadderFilter fLadder; // only used for the ladder mode
    LFO fLfo1;
    LFO fLfo2;

    EngineMode fEngineMode = EngineMode::Ring;
    FilterMode fFilterMode = FilterMode::LP12;
    float fBaseCutoff = 2000.0f;
    float fBaseRatio = 1.5f;
    float fBaseGrit = 0.5f;
    float fBaseRingMix = 1.0f;
    float fBasePulseWidth = 0.5f;
    float fSubOctave = -1.0f;
    float fSubLevel = 0.0f;
    float fVelocitySensitivity = 1.0f;

    float fModDepth[kModSourceCount][kModDestCount] = {};

    double fSampleRate = 44100.0;
    float fCurrentFreq = 110.0f;   // instantaneous pitch, moves during a glide
    float fGlideFromLog = 0.0f;    // log2(Hz) glide endpoints
    float fGlideToLog = 0.0f;
    float fGlidePos = 1.0f;        // 0..1, 1 = glide complete/inactive
    float fGlideIncrement = 1.0f;
    float fPortamentoTime = 0.0f;

    float fModWheel = 0.0f;
    float fPitchBendSemitones = 0.0f;

    int fNote = -1;
    float fVelocity = 0.0f;
    bool fActive = false;
};

} // namespace sideous
