/*
 * Sideous Bass - approximation of the SID's well-known "combined waveform"
 * hardware quirk: triangle and pulse, generated from the same phase
 * accumulator, quantized to a knob-controlled bit depth and bitwise-ANDed
 * together instead of playing cleanly. Real SID combined waveforms are a
 * documented, beloved side effect of the chip's internal DAC bit layout, not
 * a designed feature - this isn't a cycle-accurate model of that hardware,
 * just a from-scratch approximation aimed at the same recognizable
 * "bitty/digi" buzz.
 *
 * The Grit knob sweeps the quantization bit depth: low grit -> more bits,
 * a smoother/cleaner blend of the two shapes; high grit -> fewer bits, a
 * harsher, low-bit-depth crunch.
 */

#pragma once

#include <cmath>
#include <algorithm>

namespace sideous {

class CombinedOscillator
{
public:
    void setSampleRate(double sampleRate) noexcept
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    }

    void setFrequency(float hz) noexcept
    {
        fIncrement = (double)hz / fSampleRate;
    }

    // 0.01 .. 0.99, pulse component's duty cycle
    void setPulseWidth(float pw) noexcept
    {
        fPulseWidth = std::clamp(pw, 0.01f, 0.99f);
    }

    // 0 = smooth/clean (12-bit quantization), 1 = harsh chip-accurate crunch (2-bit)
    void setGrit(float grit01) noexcept
    {
        fGrit = std::clamp(grit01, 0.0f, 1.0f);
    }

    void resetPhase(float phase = 0.0f) noexcept { fPhase = phase; }

    float process() noexcept
    {
        // unsigned (0..1) triangle and pulse from the shared phase - the AND
        // trick needs unsigned bit patterns, not the usual -1..1 waveforms
        const float tri = fPhase < 0.5
                         ? (float)(fPhase * 2.0)
                         : (float)(2.0 - fPhase * 2.0);
        const float pulse = fPhase < (double)fPulseWidth ? 1.0f : 0.0f;

        const int bits = 2 + (int)((1.0f - fGrit) * 10.0f + 0.5f); // grit 0 -> 12 bits, grit 1 -> 2 bits
        const int levels = (1 << bits) - 1;

        const int triQ = (int)(tri * (float)levels + 0.5f);
        const int pulseQ = (int)(pulse * (float)levels + 0.5f);
        const int anded = triQ & pulseQ;

        const float out = ((float)anded / (float)levels) * 2.0f - 1.0f;

        fPhase += fIncrement;
        if (fPhase >= 1.0)
            fPhase -= 1.0;

        return out;
    }

private:
    double fSampleRate = 44100.0;
    double fPhase = 0.0;
    double fIncrement = 0.0;
    float fPulseWidth = 0.5f;
    float fGrit = 0.5f;
};

} // namespace sideous
