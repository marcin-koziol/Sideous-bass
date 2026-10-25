/*
 * Sideous Bass - a bass specialist with three selectable buzzy/metallic
 * 8-bit engines (Ring modulation / Hard sync / SID-style combined-waveform),
 * sub-oscillator, a multi-mode filter, and a small modular-style mod matrix
 * (two LFOs, three envelopes, and mod wheel, each freely patchable to seven
 * destinations - see dsp/BassVoice.hpp). No arpeggiator - modeled on
 * sideous's SideousPlugin.cpp, with the arp engine stripped out (not a fit
 * for a bass specialist) and the oscillator section swapped for BassVoice's
 * engine selector.
 */

#include "DistrhoPlugin.hpp"
#include "Params.hpp"
#include "dsp/BassVoice.hpp"
#include "dsp/Sync.hpp"

#include <array>
#include <cmath>

START_NAMESPACE_DISTRHO

using namespace sideous;

// -----------------------------------------------------------------------------------------------------------

static constexpr const uint32_t kNumVoices = 8; // bass is chord-occasional, not a lead - see plan §7
static constexpr const uint32_t kMaxHeldNotes = 16;

// LFO "Sync" dropdown: 0 = free-running (use the Rate Hz knob),
// 1..14 = SyncDivision::Whole..ThirtySecond
static ParameterEnumerationValue kSyncEnumValues[15] = {
    {  0.0f, "Free" },
    {  1.0f, "1/1" },
    {  2.0f, "1/2." },
    {  3.0f, "1/2" },
    {  4.0f, "1/2T" },
    {  5.0f, "1/4." },
    {  6.0f, "1/4" },
    {  7.0f, "1/4T" },
    {  8.0f, "1/8." },
    {  9.0f, "1/8" },
    { 10.0f, "1/8T" },
    { 11.0f, "1/16." },
    { 12.0f, "1/16" },
    { 13.0f, "1/16T" },
    { 14.0f, "1/32" },
};

static float syncedHz(float syncParam, float freeHz, double bpm) noexcept
{
    const int sync = (int)(syncParam + 0.5f);
    if (sync <= 0)
        return freeHz;
    return hzFromSyncDivision((SyncDivision)(sync - 1), bpm);
}

static float noteToHz(int note) noexcept
{
    return 440.0f * std::exp2(((float)note - 69.0f) / 12.0f);
}

class SideousBassPlugin : public Plugin
{
public:
    SideousBassPlugin()
        : Plugin(kParamCount, 0, 0)
    {
        sampleRateChanged(getSampleRate());
    }

protected:
    // ---------------------------------------------------------------------
    // Information

    const char* getLabel() const override { return "Sideous Bass"; }
    const char* getDescription() const override
    {
        return "Bass specialist with three selectable buzzy/metallic 8-bit engines - Ring "
               "modulation, hard Sync, and a SID-style combined-waveform approximation - a "
               "sub-oscillator, a multi-mode filter, and a small modular-style mod matrix (two "
               "LFOs, three envelopes, and mod wheel, each freely patchable to seven destinations).";
    }
    const char* getMaker() const override { return "Sideous"; }
    const char* getHomePage() const override { return DISTRHO_PLUGIN_URI; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(0, 2, 0); }

    // ---------------------------------------------------------------------
    // Init

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        const ParamInfo& info = getParamInfo(index);

        parameter.hints = kParameterIsAutomatable;
        parameter.name = info.name;
        parameter.symbol = info.symbol;
        parameter.unit = info.unit;
        parameter.ranges.min = info.min;
        parameter.ranges.max = info.max;
        parameter.ranges.def = info.def;

        // log-curved parameters reach the host as a 0..100 % position (see
        // paramToHost() in Params.hpp), so its automation sweeps them evenly
        if (isHostPosition(index))
        {
            parameter.unit = "%";
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 100.0f;
            parameter.ranges.def = paramToHost(index, info.def);
        }


        switch (index)
        {
        case kParamEngineMode:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[3] = {
                    { 0.0f, "Ring" },
                    { 1.0f, "Sync" },
                    { 2.0f, "Combined" },
                };
                parameter.enumValues.count = 3;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamWaveform:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[3] = {
                    { 0.0f, "Saw" },
                    { 1.0f, "Pulse" },
                    { 2.0f, "Triangle" },
                };
                parameter.enumValues.count = 3;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamSubOctave:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[4] = {
                    { -2.0f, "-2" },
                    { -1.0f, "-1" },
                    {  1.0f, "+1" },
                    {  2.0f, "+2" },
                };
                parameter.enumValues.count = 4;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamFilterMode:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[6] = {
                    { 0.0f, "LP 12dB" },
                    { 1.0f, "LP 24dB" },
                    { 2.0f, "HP 12dB" },
                    { 3.0f, "HP 24dB" },
                    { 4.0f, "BP 12dB" },
                    { 5.0f, "Ladder 24dB" },
                };
                parameter.enumValues.count = 6;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamLfo1Waveform:
        case kParamLfo2Waveform:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[3] = {
                    { 0.0f, "Sine" },
                    { 1.0f, "Saw" },
                    { 2.0f, "Square" },
                };
                parameter.enumValues.count = 3;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamLfo1Sync:
        case kParamLfo2Sync:
            parameter.hints |= kParameterIsInteger;
            parameter.enumValues.count = 15;
            parameter.enumValues.restrictedMode = true;
            parameter.enumValues.values = kSyncEnumValues;
            parameter.enumValues.deleteLater = false;
            break;

        case kParamPitchBendRange:
            parameter.hints |= kParameterIsInteger;
            break;

        case kParamGlideMode:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[2] = {
                    { 0.0f, "Legato" },
                    { 1.0f, "Always" },
                };
                parameter.enumValues.count = 2;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        case kParamVoiceMode:
            parameter.hints |= kParameterIsInteger;
            {
                static ParameterEnumerationValue values[2] = {
                    { 0.0f, "Poly" },
                    { 1.0f, "Mono" },
                };
                parameter.enumValues.count = 2;
                parameter.enumValues.restrictedMode = true;
                parameter.enumValues.values = values;
                parameter.enumValues.deleteLater = false;
            }
            break;

        default:
            // mod matrix cells (index >= kParamModMatrixStart) are plain
            // -1..1 depth knobs, no extra hints needed beyond the generic
            // name/symbol/range assignment above
            break;
        }
    }

    // ---------------------------------------------------------------------
    // Internal data

    float getParameterValue(uint32_t index) const override
    {
        return paramToHost(index, getEngineeringValue(index));
    }

    // a parameter in real units (Hz, s, ...), as the DSP sees it
    float getEngineeringValue(uint32_t index) const
    {
        if (index >= kParamModMatrixStart)
        {
            const uint32_t cell = index - kParamModMatrixStart;
            return fParams.modDepth[cell / kModDestCount][cell % kModDestCount];
        }

        switch (index)
        {
        case kParamEngineMode:          return (float)fParams.engineMode;
        case kParamWaveform:            return (float)fParams.waveform;
        case kParamPulseWidth:          return fParams.pulseWidth * 100.0f;
        case kParamRingSyncRatio:       return fParams.ringSyncRatio;
        case kParamRingMix:             return fParams.ringMix;
        case kParamGrit:                return fParams.grit;
        case kParamEngineAttack:        return fParams.engineAttack;
        case kParamEngineDecay:         return fParams.engineDecay;
        case kParamEngineSustain:       return fParams.engineSustain;
        case kParamEngineRelease:       return fParams.engineRelease;
        case kParamEngineEnvCurve:      return fParams.engineCurve;
        case kParamSubOctave:           return fParams.subOctave;
        case kParamSubLevel:            return fParams.subLevel;
        case kParamFilterMode:          return (float)fParams.filterMode;
        case kParamFilterCutoff:        return fParams.filterCutoff;
        case kParamFilterResonance:     return fParams.filterResonance;
        case kParamFilterDrive:         return fParams.filterDrive;
        case kParamAmpAttack:           return fParams.ampAttack;
        case kParamAmpDecay:            return fParams.ampDecay;
        case kParamAmpSustain:          return fParams.ampSustain;
        case kParamAmpRelease:          return fParams.ampRelease;
        case kParamAmpCurve:            return fParams.ampCurve;
        case kParamVelocitySensitivity: return fParams.velocitySensitivity;
        case kParamFilterAttack:        return fParams.filterAttack;
        case kParamFilterDecay:         return fParams.filterDecay;
        case kParamFilterSustain:       return fParams.filterSustain;
        case kParamFilterRelease:       return fParams.filterRelease;
        case kParamFilterEnvCurve:      return fParams.filterCurve;
        case kParamMasterVolume:        return fMasterVolume;
        case kParamLfo1Waveform:        return (float)fParams.lfo1Waveform;
        case kParamLfo1RateHz:          return fLfo1RateHz;
        case kParamLfo1Sync:            return fLfo1Sync;
        case kParamLfo2Waveform:        return (float)fParams.lfo2Waveform;
        case kParamLfo2RateHz:          return fLfo2RateHz;
        case kParamLfo2Sync:            return fLfo2Sync;
        case kParamVoiceMode:           return fMonoMode ? 1.0f : 0.0f;
        case kParamPortamentoTime:      return fParams.portamentoTime;
        case kParamPitchBendRange:      return fPitchBendRangeSemitones;
        case kParamGlideMode:           return (float)fGlideMode;
        default:                        return 0.0f;
        }
    }

    void setParameterValue(uint32_t index, float value) override
    {
        value = paramFromHost(index, value); // host position -> real units (Params.hpp)

        if (index >= kParamModMatrixStart)
        {
            const uint32_t cell = index - kParamModMatrixStart;
            fParams.modDepth[cell / kModDestCount][cell % kModDestCount] = value;
        }
        else switch (index)
        {
        case kParamEngineMode:      fParams.engineMode = (sideous::EngineMode)(int)(value + 0.5f); break;
        case kParamWaveform:
            fParams.waveform = value < 0.5f ? sideous::Waveform::Saw
                              : value < 1.5f ? sideous::Waveform::Pulse
                                              : sideous::Waveform::Triangle;
            break;
        case kParamPulseWidth:      fParams.pulseWidth = value * 0.01f; break;
        case kParamRingSyncRatio:   fParams.ringSyncRatio = value; break;
        case kParamRingMix:         fParams.ringMix = value; break;
        case kParamGrit:            fParams.grit = value; break;
        case kParamEngineAttack:    fParams.engineAttack = value; break;
        case kParamEngineDecay:     fParams.engineDecay = value; break;
        case kParamEngineSustain:   fParams.engineSustain = value; break;
        case kParamEngineRelease:   fParams.engineRelease = value; break;
        case kParamEngineEnvCurve:  fParams.engineCurve = value; break;
        case kParamSubOctave:       fParams.subOctave = value; break;
        case kParamSubLevel:        fParams.subLevel = value; break;
        case kParamFilterMode:      fParams.filterMode = (sideous::FilterMode)(int)(value + 0.5f); break;
        case kParamFilterCutoff:    fParams.filterCutoff = value; break;
        case kParamFilterResonance: fParams.filterResonance = value; break;
        case kParamFilterDrive:     fParams.filterDrive = value; break;
        case kParamAmpAttack:       fParams.ampAttack = value; break;
        case kParamAmpDecay:        fParams.ampDecay = value; break;
        case kParamAmpSustain:      fParams.ampSustain = value; break;
        case kParamAmpRelease:      fParams.ampRelease = value; break;
        case kParamAmpCurve:        fParams.ampCurve = value; break;
        case kParamVelocitySensitivity: fParams.velocitySensitivity = value; break;
        case kParamFilterAttack:    fParams.filterAttack = value; break;
        case kParamFilterDecay:     fParams.filterDecay = value; break;
        case kParamFilterSustain:   fParams.filterSustain = value; break;
        case kParamFilterRelease:   fParams.filterRelease = value; break;
        case kParamFilterEnvCurve:  fParams.filterCurve = value; break;
        case kParamMasterVolume:    fMasterVolume = value; break;
        case kParamLfo1Waveform:    fParams.lfo1Waveform = (sideous::LfoWaveform)(int)(value + 0.5f); break;
        case kParamLfo1RateHz:      fLfo1RateHz = value; break;
        case kParamLfo1Sync:        fLfo1Sync = value; break;
        case kParamLfo2Waveform:    fParams.lfo2Waveform = (sideous::LfoWaveform)(int)(value + 0.5f); break;
        case kParamLfo2RateHz:      fLfo2RateHz = value; break;
        case kParamLfo2Sync:        fLfo2Sync = value; break;
        case kParamVoiceMode:
            {
                const bool newMono = value >= 0.5f;
                if (newMono != fMonoMode)
                {
                    // don't leave orphaned voices ringing when switching modes
                    for (sideous::BassVoice& voice : fVoices)
                        voice.kill();
                    fMonoVoice.kill();
                    fMonoStackCount = 0;
                }
                fMonoMode = newMono;
            }
            break;
        case kParamPortamentoTime:  fParams.portamentoTime = value; break;
        case kParamPitchBendRange:  fPitchBendRangeSemitones = value; break;
        case kParamGlideMode:       fGlideMode = (int)(value + 0.5f); break;
        default: return;
        }

        for (sideous::BassVoice& voice : fVoices)
            voice.applyParams(fParams);
        fMonoVoice.applyParams(fParams);
    }

    // ---------------------------------------------------------------------
    // Audio/MIDI Processing

    void activate() override
    {
        sampleRateChanged(getSampleRate());
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fSampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        for (sideous::BassVoice& voice : fVoices)
        {
            voice.setSampleRate(newSampleRate);
            voice.applyParams(fParams);
        }
        fMonoVoice.setSampleRate(newSampleRate);
        fMonoVoice.applyParams(fParams);
    }

    void run(const float**, float** outputs, uint32_t frames,
             const MidiEvent* midiEvents, uint32_t midiEventCount) override
    {
        float* outL = outputs[0];
        float* outR = outputs[1];

        const TimePosition& timePos = getTimePosition();
        const double bpm = timePos.bbt.valid && timePos.bbt.beatsPerMinute > 0.0
                          ? timePos.bbt.beatsPerMinute : 120.0;

        const float lfo1Hz = syncedHz(fLfo1Sync, fLfo1RateHz, bpm);
        const float lfo2Hz = syncedHz(fLfo2Sync, fLfo2RateHz, bpm);
        for (sideous::BassVoice& voice : fVoices)
        {
            voice.setLfo1Frequency(lfo1Hz);
            voice.setLfo2Frequency(lfo2Hz);
        }
        fMonoVoice.setLfo1Frequency(lfo1Hz);
        fMonoVoice.setLfo2Frequency(lfo2Hz);

        uint32_t nextEvent = 0;

        for (uint32_t frame = 0; frame < frames; ++frame)
        {
            bool controllersChanged = false;
            while (nextEvent < midiEventCount && midiEvents[nextEvent].frame == frame)
            {
                const uint8_t statusBefore = midiEvents[nextEvent].data[0] & 0xF0;
                if (statusBefore == 0xE0 || statusBefore == 0xB0)
                    controllersChanged = true;
                handleMidiEvent(midiEvents[nextEvent]);
                ++nextEvent;
            }

            if (controllersChanged)
            {
                const float pitchBendSemitones = fPitchBendNormalized * fPitchBendRangeSemitones;
                for (sideous::BassVoice& voice : fVoices)
                {
                    voice.setPitchBend(pitchBendSemitones);
                    voice.setModWheel(fModWheelValue);
                }
                fMonoVoice.setPitchBend(pitchBendSemitones);
                fMonoVoice.setModWheel(fModWheelValue);
            }

            float mix = 0.0f;
            for (sideous::BassVoice& voice : fVoices)
                mix += voice.process();
            mix += fMonoVoice.process();

            // mono mode (the default - basslines are conventionally
            // monophonic) never has more than one voice sounding: glide/
            // legato retrigger the same fMonoVoice, they don't stack like
            // poly mode's up-to-8 fVoices can. Using poly's worst-case
            // headroom unconditionally made the instrument's own default
            // patch needlessly quiet - kPolyHeadroom stays conservative for
            // when chords actually can stack.
            const float headroom = fMonoMode ? kMonoHeadroom : kPolyHeadroom;
            mix *= fMasterVolume * headroom;

            // gentle safety saturation: Ring/Sync push extra harmonic energy
            // into a resonant filter compared to a plain saw/pulse, and can
            // otherwise blow well past 0dBFS - see plan §4
            mix = std::tanh(mix);

            outL[frame] = mix;
            outR[frame] = mix;
        }

        // catch any remaining events stamped past the last frame (shouldn't happen, but be safe)
        while (nextEvent < midiEventCount)
            handleMidiEvent(midiEvents[nextEvent++]);
    }

private:
    static constexpr const float kMonoHeadroom = 0.85f;
    static constexpr const float kPolyHeadroom = 1.0f / 3.0f;

    void handleMidiEvent(const MidiEvent& event) noexcept
    {
        if (event.size < 2 || event.size > 3)
            return;

        const uint8_t status = event.data[0] & 0xF0;
        const uint8_t note = event.data[1];
        const uint8_t velocity = event.size > 2 ? event.data[2] : 0;

        if (status == 0x90 && velocity > 0)
        {
            if (fMonoMode)
                monoNoteOn(note, velocity);
            else
                triggerVoiceNoteOn(note, velocity);
        }
        else if (status == 0x80 || (status == 0x90 && velocity == 0))
        {
            if (fMonoMode)
                monoNoteOff(note);
            else
                triggerVoiceNoteOff(note);
        }
        else if (status == 0xE0 && event.size >= 3) // pitch bend, 14-bit, center = 8192
        {
            const int raw = (int)event.data[1] | ((int)event.data[2] << 7);
            float normalized = ((float)raw - 8192.0f) / 8192.0f;
            fPitchBendNormalized = normalized < -1.0f ? -1.0f : (normalized > 1.0f ? 1.0f : normalized);
        }
        else if (status == 0xB0 && event.size >= 3 && event.data[1] == 1) // CC1 = mod wheel
        {
            fModWheelValue = (float)event.data[2] / 127.0f;
        }
    }

    // ---------------------------------------------------------------------
    // voice allocation. Transparently routes to the single mono voice when
    // mono mode is active: retriggerEnvelope becomes false whenever that
    // voice is already sounding, which is correct for overlapping ("legato")
    // playing. Direct MIDI input in mono mode does NOT come through here for
    // note-on - see monoNoteOn(), which needs the real held-key stack to get
    // retriggering right.

    // computes the glideFromFreq sentinel for BassVoice::noteOn() from the
    // current Glide Mode: Always glides from whatever note last played,
    // Legato leaves it to BassVoice's own "only if already active" default.
    float glideSourceForNextNote() const noexcept
    {
        if (fGlideMode == 1) // Always
            return fHasLastNoteFreq ? fLastNoteFreq : 0.0f;
        return -1.0f; // Legato: auto
    }

    void triggerVoiceNoteOn(int note, int velocity) noexcept
    {
        if (fMonoMode)
        {
            const bool retrigger = !fMonoVoice.isActive();
            fMonoVoice.applyParams(fParams);
            fMonoVoice.noteOn(note, (float)velocity / 127.0f, retrigger, glideSourceForNextNote());
            fLastNoteFreq = noteToHz(note);
            fHasLastNoteFreq = true;
            return;
        }

        sideous::BassVoice* target = nullptr;

        for (sideous::BassVoice& voice : fVoices)
        {
            if (!voice.isActive())
            {
                target = &voice;
                break;
            }
        }

        if (target == nullptr)
        {
            for (sideous::BassVoice& voice : fVoices)
            {
                if (voice.isReleasing())
                {
                    target = &voice;
                    break;
                }
            }
        }

        if (target == nullptr)
            target = &fVoices[fStealCursor++ % kNumVoices];

        target->applyParams(fParams);
        target->noteOn(note, (float)velocity / 127.0f, true, glideSourceForNextNote());

        fLastNoteFreq = noteToHz(note);
        fHasLastNoteFreq = true;
    }

    void triggerVoiceNoteOff(int note) noexcept
    {
        if (fMonoMode)
        {
            if (fMonoVoice.getNote() == note)
                fMonoVoice.noteOff();
            return;
        }

        for (sideous::BassVoice& voice : fVoices)
        {
            if (voice.isActive() && voice.getNote() == note && !voice.isReleasing())
                voice.noteOff();
        }
    }

    // ---------------------------------------------------------------------
    // mono mode note stack: tracks which real keys are physically held so
    // releasing the current note can fall back to the previous one, like a
    // classic analog mono synth.
    //
    // Retriggering is decided from the stack transition (0 held -> 1 held),
    // NOT from whether the voice is still audible. A voice can still be
    // ringing out a long release well after its key was let go; using
    // isActive() there would treat a fresh staccato attack as a legato
    // continuation and just keep fading out the old envelope instead of
    // re-attacking, which is silent/broken for anything but very short
    // release times.

    void monoNoteOn(int note, int velocity) noexcept
    {
        for (uint32_t i = 0; i < fMonoStackCount; ++i)
        {
            if (fMonoStack[i].note == note)
            {
                removeMonoStackIndex(i);
                break;
            }
        }

        const bool wasEmpty = fMonoStackCount == 0;
        if (fMonoStackCount < kMaxHeldNotes)
            fMonoStack[fMonoStackCount++] = { note, velocity };

        const bool retrigger = wasEmpty;
        float glideFrom;
        if (fGlideMode == 1) // Always
            glideFrom = fHasLastNoteFreq ? fLastNoteFreq : 0.0f;
        else // Legato: glide only on genuine overlap (stack wasn't empty)
            glideFrom = wasEmpty ? 0.0f : -1.0f;

        fMonoVoice.applyParams(fParams);
        fMonoVoice.noteOn(note, (float)velocity / 127.0f, retrigger, glideFrom);

        fLastNoteFreq = noteToHz(note);
        fHasLastNoteFreq = true;
    }

    void monoNoteOff(int note) noexcept
    {
        for (uint32_t i = 0; i < fMonoStackCount; ++i)
        {
            if (fMonoStack[i].note == note)
            {
                removeMonoStackIndex(i);
                break;
            }
        }

        if (fMonoStackCount == 0)
            fMonoVoice.noteOff();
        else
            triggerVoiceNoteOn(fMonoStack[fMonoStackCount - 1].note, fMonoStack[fMonoStackCount - 1].velocity);
    }

    void removeMonoStackIndex(uint32_t index) noexcept
    {
        for (uint32_t j = index; j < fMonoStackCount - 1; ++j)
            fMonoStack[j] = fMonoStack[j + 1];
        --fMonoStackCount;
    }

    struct HeldNote { int note; int velocity; };

    std::array<sideous::BassVoice, kNumVoices> fVoices;
    sideous::VoiceParams fParams;
    float fMasterVolume = 0.8f;
    uint32_t fStealCursor = 0;
    double fSampleRate = 44100.0;
    float fLastNoteFreq = 0.0f;    // poly portamento glide source, see triggerVoiceNoteOn()
    bool fHasLastNoteFreq = false;
    int fGlideMode = 0;             // 0 = Legato, 1 = Always (see kSyncEnumValues-style table)

    float fPitchBendNormalized = 0.0f; // -1..1, from MIDI pitch bend (0xE0)
    float fPitchBendRangeSemitones = 2.0f;
    float fModWheelValue = 0.0f;       // 0..1, from MIDI CC1 - a Mod Matrix source, see dsp/BassVoice.hpp

    float fLfo1RateHz = 2.0f;
    float fLfo1Sync = 0.0f; // 0 = free, see kSyncEnumValues
    float fLfo2RateHz = 3.0f;
    float fLfo2Sync = 0.0f; // 0 = free, see kSyncEnumValues

    bool fMonoMode = true; // basslines are conventionally monophonic - see plan §7
    sideous::BassVoice fMonoVoice;
    std::array<HeldNote, kMaxHeldNotes> fMonoStack;
    uint32_t fMonoStackCount = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SideousBassPlugin)
};

// -----------------------------------------------------------------------------------------------------------

Plugin* createPlugin()
{
    return new SideousBassPlugin();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
