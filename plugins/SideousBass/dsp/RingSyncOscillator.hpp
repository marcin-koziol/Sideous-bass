/*
 * Sideous Bass - two-oscillator engine for the Ring and Sync bass character.
 * Carrier tracks the note pitch; a second "modulator/slave" oscillator runs
 * at a knob-controlled ratio of the carrier frequency.
 *
 *   Ring: out = mix(carrier, carrier * modulator, depth) - the modulator
 *         free-runs at its own ratio-derived rate with no phase interaction,
 *         giving the classic inharmonic SID "clang".
 *   Sync: the modulator's phase is force-reset every time the carrier's
 *         phase wraps ("hard sync"); output is the modulator alone, whose
 *         swept harmonic content is set by the Ratio knob - the buzzing,
 *         "screaming" sync-bass character.
 */

#pragma once

#include <cmath>

#include "Oscillator.hpp"

namespace sideous {

enum class RingSyncMode { Ring = 0, Sync };

class RingSyncOscillator
{
public:
    void setSampleRate(double sampleRate) noexcept
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        fCarrier.setSampleRate(sampleRate);
        fModulator.setSampleRate(sampleRate);
    }

    void setMode(RingSyncMode m) noexcept { fMode = m; }

    void setWaveform(Waveform w) noexcept
    {
        fCarrier.setWaveform(w);
        fModulator.setWaveform(w);
    }

    void setPulseWidth(float pw) noexcept
    {
        fCarrier.setPulseWidth(pw);
        fModulator.setPulseWidth(pw);
    }

    // modulator/slave frequency = carrier frequency * ratio
    void setRatio(float ratio) noexcept
    {
        fRatio = ratio < 0.5f ? 0.5f : (ratio > 8.0f ? 8.0f : ratio);
    }

    // Ring only: 0 = dry carrier, 1 = full ring product
    void setMix(float mix01) noexcept
    {
        fMix = mix01 < 0.0f ? 0.0f : (mix01 > 1.0f ? 1.0f : mix01);
    }

    void setFrequency(float hz) noexcept
    {
        fCarrier.setFrequency(hz);
        fModulator.setFrequency(hz * fRatio);
        fCarrierIncrement = (double)hz / fSampleRate;
    }

    void resetPhase() noexcept
    {
        fCarrier.resetPhase();
        fModulator.resetPhase();
        fCarrierPhase = 0.0;
    }

    float process() noexcept
    {
        const float carrierOut = fCarrier.process();

        if (fMode == RingSyncMode::Sync)
        {
            // Oscillator.hpp doesn't expose its own phase-wrap events, so a
            // shadow phase counter tracks the carrier's wrap independently
            // (same increment, same wrap point) purely to know when to
            // hard-reset the modulator - avoids modifying the shared class.
            fCarrierPhase += fCarrierIncrement;
            if (fCarrierPhase >= 1.0)
            {
                fCarrierPhase -= 1.0;
                // anti-click: reset the modulator to where its own phase
                // would already be at the exact sub-sample instant the
                // carrier wrapped, not hard to 0.0 - a naive always-reset-to-
                // zero throws a broadband click into the signal every single
                // carrier cycle. This keeps the reset sub-sample-accurate,
                // which measurably softens (though doesn't fully band-limit)
                // that click.
                const float resetPhase = (float)std::fmod(fCarrierPhase * fRatio, 1.0);
                fModulator.resetPhase(resetPhase);
            }
            return fModulator.process();
        }

        // Ring
        const float modulatorOut = fModulator.process();
        return carrierOut + (carrierOut * modulatorOut - carrierOut) * fMix;
    }

private:
    double fSampleRate = 44100.0;
    Oscillator fCarrier;
    Oscillator fModulator;
    RingSyncMode fMode = RingSyncMode::Ring;
    float fRatio = 1.5f;
    float fMix = 1.0f;

    double fCarrierPhase = 0.0;
    double fCarrierIncrement = 0.0;
};

} // namespace sideous
