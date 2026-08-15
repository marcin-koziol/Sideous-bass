/*
 * Sideous Bass - GUI layout and Cairo painting, shared between the real
 * plugin UI and an offline PNG renderer used to check the look during
 * development.
 *
 * Industrial/metallic styling for a bass specialist built on buzzy 8-bit
 * tricks: gunmetal panels, brushed-steel/copper accent colors, one CRT
 * phosphor-green "signature" accent tying the ENGINE panel (Ring/Sync/
 * Combined) to the PATCH BAY (the modular-style mod matrix, this
 * instrument's other core concept). Each of the six mod matrix sources gets
 * its own vivid "patch cable" color, reused consistently for its home panel
 * accent where it has one (LFO1/LFO2/Amp Env/Filter Env/Engine Env) so a
 * cable's color always identifies its source at a glance. Widget-drawing
 * mechanics (paintKnob, paintSelector, paintDropdown, paintEnvelopeGraph,
 * roundedRect/drawText helpers) are copied wholesale from sideous - only the
 * palette, buildLayout(), and the patch-bay-specific paint functions are new.
 */

#pragma once

#include <cairo.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>
#include <vector>

#include "../Params.hpp"

namespace sideous {
namespace ui {

// -----------------------------------------------------------------------------------------------------------
// palette - gunmetal/industrial chassis, CRT-scope green as the signature accent

struct Color { double r, g, b; };

static constexpr Color kBg        { 0.043, 0.047, 0.055 }; // near-black, faint blue-grey tint
static constexpr Color kPanelBg   { 0.106, 0.114, 0.129 }; // dark gunmetal panel fill
static constexpr Color kPanelEdge { 0.302, 0.322, 0.353 }; // brushed-steel panel outline
static constexpr Color kTextMain  { 0.902, 0.910, 0.925 }; // near-white, contrast on dark bg
static constexpr Color kTextDim   { 0.549, 0.569, 0.600 }; // muted steel grey, secondary text
static constexpr Color kKnobBody  { 0.145, 0.153, 0.173 }; // knob face
static constexpr Color kKnobRing  { 0.239, 0.255, 0.282 }; // knob track (unfilled arc)
static constexpr Color kBtnBg     { 0.122, 0.129, 0.145 }; // unselected selector/dropdown fill

static constexpr Color kSteelBlue { 0.353, 0.573, 0.788 }; // Oscillator
static constexpr Color kCrtGreen  { 0.463, 0.902, 0.290 }; // Engine / Engine Env / Patch Bay (signature)
static constexpr Color kCopper    { 0.824, 0.541, 0.220 }; // Filter
static constexpr Color kBrass     { 0.851, 0.698, 0.235 }; // Amp Env
static constexpr Color kRust      { 0.776, 0.341, 0.180 }; // Filter Env
static constexpr Color kChrome    { 0.290, 0.788, 0.769 }; // LFO1
static constexpr Color kAmber     { 0.937, 0.600, 0.220 }; // LFO2
static constexpr Color kMagenta   { 0.870, 0.310, 0.580 }; // Mod Wheel (patch-bay-only, no dedicated panel)

inline void setColor(cairo_t* cr, Color c, double a = 1.0) noexcept
{
    cairo_set_source_rgba(cr, c.r, c.g, c.b, a);
}

// -----------------------------------------------------------------------------------------------------------
// layout model

struct Knob
{
    uint32_t param;
    float cx, cy, radius;
    const char* label;
    Color accent;
};

struct SelectorOption
{
    float value;
    const char* label;
};

struct Selector
{
    uint32_t param;
    float x, y, w, h;
    std::vector<SelectorOption> options;
    Color accent;
    const char* caption = nullptr; // optional small label drawn above the row,
                                    // for selectors that live outside a titled panel
};

// like a Selector, but the options are hidden behind a closed combo-box until
// clicked open - used where there are too many options for a button row
struct Dropdown
{
    uint32_t param;
    float x, y, w, h;
    std::vector<SelectorOption> options;
    Color accent;
};

struct PanelBox { float x, y, w, h; const char* title; Color accent; };

// read-only ADSR shape preview - no interaction, just visualizes the
// attack/decay/sustain/release/curve parameters of one envelope
struct EnvelopeGraph
{
    uint32_t attackParam, decayParam, sustainParam, releaseParam, curveParam;
    float x, y, w, h;
    Color accent;
};

// mod matrix source/destination counts and labels - must stay in sync with
// dsp/BassVoice.hpp's ModSource/ModDest enums and Params.hpp's
// modMatrixParam() row-major layout.
static constexpr int kModSourceCount = 6;
static constexpr int kModDestCount = 7;

struct ModSourceInfo { const char* label; Color accent; };
static const ModSourceInfo kModSources[kModSourceCount] = {
    { "LFO1",       kChrome },
    { "LFO2",       kAmber },
    { "AMP ENV",    kBrass },
    { "FILTER ENV", kRust },
    { "ENGINE ENV", kCrtGreen },
    { "MOD WHEEL",  kMagenta },
};
static const char* const kModDests[kModDestCount] = {
    "PITCH", "CUTOFF", "RATIO", "GRIT", "RING MIX", "PW", "AMP",
};

// a single jack socket in the patch bay - a source (left edge) or
// destination (top header), drawn as a small circle + label, with cables
// (see paintPatchCables) drawn to/from it for every active connection.
struct ModJack { float x, y; Color accent; const char* label; };

struct PatchBayLayout
{
    ModJack sources[kModSourceCount];
    ModJack dests[kModDestCount];
    // the one grid cell that isn't a knob: Amp Env -> Amplitude is always
    // connected (Amp Env is the voice's hardwired VCA - see dsp/BassVoice.hpp)
    // and not user-patchable, so it's drawn as a fixed indicator here
    // instead of an adjustable depth knob.
    ModJack ampEnvAlwaysOn;
};

// a plain clickable button, not tied to a DPF parameter - presets are pure
// UI-side state (see ui/PresetStore.hpp), so Prev/Next/Save/Delete can't
// reuse the param-driven Selector/Knob widgets the rest of the UI is built
// from.
struct Button { float x, y, w, h; const char* label; Color accent; };

struct PresetBarLayout
{
    Button prev, next, save, del;
    float nameX, nameY, nameW, nameH;
    Color accent;
};

struct Layout
{
    float width, height;
    std::vector<PanelBox> panels;
    std::vector<Knob> knobs;
    std::vector<Selector> selectors;
    std::vector<Dropdown> dropdowns;
    std::vector<EnvelopeGraph> envelopeGraphs;
    PatchBayLayout patchBay;
    PresetBarLayout presetBar;
};

// -----------------------------------------------------------------------------------------------------------
// layout builder

inline void addKnobRow(std::vector<Knob>& knobs, float panelX, float panelW, float rowY, float radius,
                        Color accent, std::initializer_list<std::pair<uint32_t, const char*>> items)
{
    const float slot = panelW / (float)items.size();
    size_t i = 0;
    for (const auto& item : items)
    {
        const float cx = panelX + slot * ((float)i + 0.5f);
        knobs.push_back({ item.first, cx, rowY, radius, item.second, accent });
        ++i;
    }
}

// shared by the LFO sync dropdowns: 0 = free-running (use the paired Rate Hz
// knob), 1..14 = tempo note divisions (dotted/straight/triplet)
inline std::vector<SelectorOption> syncOptions()
{
    return {
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
}

inline Layout buildLayout(float width, float height)
{
    Layout L;
    L.width = width;
    L.height = height;

    // wide-and-short rather than narrow-and-tall (same rework as sideous):
    // a 3-column grid for the simple panels, 2-column for the knob-heavy
    // envelopes, and the patch bay given the *full* width instead of being
    // squeezed into a tall narrow column - it only ever needed 7 destination
    // rows of vertical space regardless of column width, so widening it
    // shrinks its height for free instead of needing 400px to breathe.
    const float margin = 12.0f;
    const float gap = 8.0f;
    const float fullW = width - margin * 2.0f;
    const float colW = (fullW - gap) / 2.0f;             // 2-col rows: envelopes, LFO1/LFO2
    const float colBx = margin + colW + gap;
    const float col3W = (fullW - gap * 2.0f) / 3.0f;      // 3-col row: OSC/FILTER/ENGINE
    const float col3Bx = margin + col3W + gap;
    const float col3Cx = col3Bx + col3W + gap;

    // preset bar: full width, right below the header; everything else starts below it
    const float presetBarY = 48.0f, presetBarH = 40.0f;

    const float row1Y = presetBarY + presetBarH + gap, row1H = 136.0f;  // OSC/FILTER/ENGINE
    const float row2Y = row1Y + row1H + gap, row2H = 146.0f;            // AMP/FILTER/ENGINE ENV
    const float row3Y = row2Y + row2H + gap, row3H = 162.0f;            // LFO1/LFO2
    const float row4Y = row3Y + row3H + gap, row4H = 260.0f;            // PATCH BAY
    const float barY = row4Y + row4H + gap, barH = height - barY - margin;
    const float totalContentW = fullW;

    // --- preset bar --- (all positions absolute, left-to-right: prev/next,
    // then the name field filling the middle, then save/delete flush right)
    {
        L.presetBar.accent = kCrtGreen;
        const float y = presetBarY, h = presetBarH;
        const float barLeftX = margin;
        const float barRightX = margin + totalContentW;

        L.presetBar.prev = { barLeftX, y, 40.0f, h, "<", kCrtGreen };
        L.presetBar.next = { barLeftX + 40.0f + 8.0f, y, 40.0f, h, ">", kCrtGreen };

        const float delX = barRightX - 100.0f;
        const float saveX = delX - 8.0f - 90.0f;
        L.presetBar.save = { saveX, y, 90.0f, h, "SAVE", kCrtGreen };
        L.presetBar.del  = { delX, y, 100.0f, h, "DELETE", kRust };

        L.presetBar.nameX = L.presetBar.next.x + 40.0f + 14.0f;
        L.presetBar.nameY = y;
        L.presetBar.nameW = saveX - 14.0f - L.presetBar.nameX;
        L.presetBar.nameH = h;
    }

    L.panels.push_back({ margin,  row1Y, col3W, row1H, "OSCILLATOR", kSteelBlue }); // 0
    L.panels.push_back({ col3Bx,  row1Y, col3W, row1H, "FILTER",     kCopper });    // 1
    L.panels.push_back({ margin,  row2Y, col3W, row2H, "AMP ENV",    kBrass });     // 2
    L.panels.push_back({ col3Bx,  row2Y, col3W, row2H, "FILTER ENV", kRust });      // 3
    L.panels.push_back({ col3Cx,  row1Y, col3W, row1H, "ENGINE",     kCrtGreen });  // 4
    L.panels.push_back({ col3Cx,  row2Y, col3W, row2H, "ENGINE ENV", kCrtGreen });  // 5
    L.panels.push_back({ margin,  row3Y, colW,  row3H, "LFO1",       kChrome });    // 6
    L.panels.push_back({ colBx,   row3Y, colW,  row3H, "LFO2",       kAmber });     // 7
    L.panels.push_back({ margin,  row4Y, fullW, row4H, "PATCH BAY",  kCrtGreen });  // 8
    L.panels.push_back({ margin,  barY,  fullW, barH,  nullptr, kCrtGreen });       // 9

    // --- oscillator ---
    {
        const PanelBox& p = L.panels[0];
        Selector wave;
        wave.param = kParamWaveform;
        wave.accent = p.accent;
        wave.x = p.x + 12.0f; wave.y = p.y + 26.0f; wave.w = p.w - 24.0f; wave.h = 24.0f;
        wave.options = { { 0.0f, "SAW" }, { 1.0f, "PULSE" }, { 2.0f, "TRI" } };
        L.selectors.push_back(wave);

        const float rowY = p.y + 26.0f + 24.0f + 40.0f;

        // PW knob | sub-osc octave selector | sub-osc level knob, side by side
        L.knobs.push_back({ kParamPulseWidth, p.x + p.w * 0.16f, rowY, 22.0f, "PW", p.accent });

        Selector subOct;
        subOct.param = kParamSubOctave;
        subOct.accent = p.accent;
        subOct.w = p.w * 0.40f;
        subOct.x = p.x + p.w * 0.5f - subOct.w * 0.5f;
        subOct.h = 24.0f;
        subOct.y = rowY - subOct.h * 0.5f;
        subOct.options = { { -2.0f, "-2" }, { -1.0f, "-1" }, { 1.0f, "+1" }, { 2.0f, "+2" } };
        L.selectors.push_back(subOct);

        L.knobs.push_back({ kParamSubLevel, p.x + p.w * 0.84f, rowY, 22.0f, "SUB LVL", p.accent });
    }

    // --- filter --- (Cutoff/Reso/Drive only - envelope amount is now
    // patched via the Filter Env -> Cutoff mod matrix cell)
    {
        const PanelBox& p = L.panels[1];
        Dropdown mode;
        mode.param = kParamFilterMode;
        mode.accent = p.accent;
        mode.x = p.x + 12.0f; mode.y = p.y + 26.0f; mode.w = p.w - 24.0f; mode.h = 24.0f;
        mode.options = {
            { 0.0f, "LOWPASS 12dB" },
            { 1.0f, "LOWPASS 24dB" },
            { 2.0f, "HIGHPASS 12dB" },
            { 3.0f, "HIGHPASS 24dB" },
            { 4.0f, "BANDPASS 12dB" },
            { 5.0f, "LADDER 24dB" },
        };
        L.dropdowns.push_back(mode);

        addKnobRow(L.knobs, p.x, p.w, p.y + 26.0f + 24.0f + 40.0f, 22.0f, p.accent,
                   {
                       { kParamFilterCutoff,    "CUTOFF" },
                       { kParamFilterResonance, "RESO" },
                       { kParamFilterDrive,     "DRIVE" },
                   });
    }

    // --- amp envelope --- (always the voice's own VCA, not patchable away -
    // see dsp/BassVoice.hpp; also available as a mod matrix source for
    // everything else)
    {
        const PanelBox& p = L.panels[2];
        const float rowY = p.y + 26.0f + 30.0f;
        addKnobRow(L.knobs, p.x, p.w, rowY, 20.0f, p.accent,
                   {
                       { kParamAmpAttack,           "ATTACK" },
                       { kParamAmpDecay,             "DECAY" },
                       { kParamAmpSustain,           "SUSTAIN" },
                       { kParamAmpRelease,           "RELEASE" },
                       { kParamAmpCurve,             "CURVE" },
                       { kParamVelocitySensitivity,  "VELOCITY" },
                   });

        L.envelopeGraphs.push_back({
            kParamAmpAttack, kParamAmpDecay, kParamAmpSustain, kParamAmpRelease, kParamAmpCurve,
            p.x + 12.0f, rowY + 20.0f + 22.0f, p.w - 24.0f, 38.0f, p.accent });
    }

    // --- filter envelope --- (generator only; what it modulates is decided
    // by the mod matrix)
    {
        const PanelBox& p = L.panels[3];
        const float rowY = p.y + 26.0f + 30.0f;
        addKnobRow(L.knobs, p.x, p.w, rowY, 20.0f, p.accent,
                   {
                       { kParamFilterAttack,   "ATTACK" },
                       { kParamFilterDecay,    "DECAY" },
                       { kParamFilterSustain,  "SUSTAIN" },
                       { kParamFilterRelease,  "RELEASE" },
                       { kParamFilterEnvCurve, "CURVE" },
                   });

        L.envelopeGraphs.push_back({
            kParamFilterAttack, kParamFilterDecay, kParamFilterSustain, kParamFilterRelease, kParamFilterEnvCurve,
            p.x + 12.0f, rowY + 20.0f + 22.0f, p.w - 24.0f, 38.0f, p.accent });
    }

    // --- engine (Ring / Sync / Combined) - the instrument's core concept ---
    {
        const PanelBox& p = L.panels[4];

        Selector engine;
        engine.param = kParamEngineMode;
        engine.accent = p.accent;
        engine.x = p.x + 12.0f; engine.y = p.y + 26.0f; engine.w = p.w - 24.0f; engine.h = 24.0f;
        engine.options = { { 0.0f, "RING" }, { 1.0f, "SYNC" }, { 2.0f, "COMBINED" } };
        L.selectors.push_back(engine);

        // all three knobs stay visible regardless of the current Engine
        // selection (same convention as sideous-pad's Unison/Formant
        // panels) - Ratio drives Ring and Sync, Ring Mix only Ring, Grit
        // only Combined; the labels make each knob's relevance obvious.
        addKnobRow(L.knobs, p.x, p.w, engine.y + engine.h + 40.0f, 22.0f, p.accent,
                   {
                       { kParamRingSyncRatio, "RATIO" },
                       { kParamRingMix,       "RING MIX" },
                       { kParamGrit,          "GRIT" },
                   });
    }

    // --- engine envelope --- (generator only; sweeps Ratio via the mod
    // matrix's Engine Env -> Ratio cell - the classic "swept sync" bass move)
    {
        const PanelBox& p = L.panels[5];
        const float rowY = p.y + 26.0f + 30.0f;
        addKnobRow(L.knobs, p.x, p.w, rowY, 20.0f, p.accent,
                   {
                       { kParamEngineAttack,    "ATTACK" },
                       { kParamEngineDecay,      "DECAY" },
                       { kParamEngineSustain,    "SUSTAIN" },
                       { kParamEngineRelease,    "RELEASE" },
                       { kParamEngineEnvCurve,   "CURVE" },
                   });

        L.envelopeGraphs.push_back({
            kParamEngineAttack, kParamEngineDecay, kParamEngineSustain, kParamEngineRelease, kParamEngineEnvCurve,
            p.x + 12.0f, rowY + 20.0f + 22.0f, p.w - 24.0f, 38.0f, p.accent });
    }

    // --- LFO1 / LFO2 --- (generators only; destinations are entirely the
    // mod matrix's job now, no Destination selector or Amount knob here)
    {
        const PanelBox& p = L.panels[6];
        Selector wave;
        wave.param = kParamLfo1Waveform;
        wave.accent = p.accent;
        wave.x = p.x + 12.0f; wave.y = p.y + 26.0f; wave.w = p.w - 24.0f; wave.h = 24.0f;
        wave.options = { { 0.0f, "SINE" }, { 1.0f, "SAW" }, { 2.0f, "SQR" } };
        L.selectors.push_back(wave);

        Dropdown sync;
        sync.param = kParamLfo1Sync;
        sync.accent = p.accent;
        sync.x = p.x + 12.0f; sync.y = wave.y + wave.h + 8.0f; sync.w = p.w - 24.0f; sync.h = 24.0f;
        sync.options = syncOptions();
        L.dropdowns.push_back(sync);

        L.knobs.push_back({ kParamLfo1RateHz, p.x + p.w * 0.5f, sync.y + sync.h + 32.0f, 22.0f, "RATE", p.accent });
    }
    {
        const PanelBox& p = L.panels[7];
        Selector wave;
        wave.param = kParamLfo2Waveform;
        wave.accent = p.accent;
        wave.x = p.x + 12.0f; wave.y = p.y + 26.0f; wave.w = p.w - 24.0f; wave.h = 24.0f;
        wave.options = { { 0.0f, "SINE" }, { 1.0f, "SAW" }, { 2.0f, "SQR" } };
        L.selectors.push_back(wave);

        Dropdown sync;
        sync.param = kParamLfo2Sync;
        sync.accent = p.accent;
        sync.x = p.x + 12.0f; sync.y = wave.y + wave.h + 8.0f; sync.w = p.w - 24.0f; sync.h = 24.0f;
        sync.options = syncOptions();
        L.dropdowns.push_back(sync);

        L.knobs.push_back({ kParamLfo2RateHz, p.x + p.w * 0.5f, sync.y + sync.h + 32.0f, 22.0f, "RATE", p.accent });
    }

    // --- patch bay --- (the mod matrix: 6 sources x 7 destinations, all
    // depths default to 0/no connection - see dsp/BassVoice.hpp). Now that
    // this panel lives in a tall, narrow right-hand column rather than
    // spanning the full window width, the grid is transposed from its
    // original shape: 6 source jacks across a header row along the top (a
    // narrow column has plenty of *vertical* room for the 7-item axis but
    // not much *horizontal* room for it), 7 destination jacks down the left
    // edge. Each (source, dest) intersection is both a tiny depth knob (drag
    // to connect/adjust, double-click to disconnect back to 0 - identical
    // mechanics to every other knob, just tiny and unlabeled) and, once
    // non-zero, the point a cable is drawn through in paintPatchCables().
    {
        const PanelBox& p = L.panels[8];

        const float headerY = p.y + 34.0f;      // source jack row
        const float gridLeftX = p.x + 90.0f;    // first source column
        const float gridRightX = p.x + p.w - 16.0f;
        const float gridTop = headerY + 30.0f;  // first destination row
        const float gridBottom = p.y + p.h - 14.0f;

        for (int s = 0; s < kModSourceCount; ++s)
        {
            const float x = gridLeftX + (gridRightX - gridLeftX) * ((float)s + 0.5f) / (float)kModSourceCount;
            L.patchBay.sources[s] = { x, headerY, kModSources[s].accent, kModSources[s].label };
        }

        for (int d = 0; d < kModDestCount; ++d)
        {
            const float y = gridTop + (gridBottom - gridTop) * ((float)d + 0.5f) / (float)kModDestCount;
            L.patchBay.dests[d] = { p.x + 18.0f, y, kTextMain, kModDests[d] };

            for (int s = 0; s < kModSourceCount; ++s)
            {
                const float x = L.patchBay.sources[s].x;
                // s==2 (AmpEnv) / d==6 (Amplitude): see PatchBayLayout::ampEnvAlwaysOn.
                // Plain indices, not the ModSource/ModDest enums, because
                // those live in dsp/BassVoice.hpp which this UI-only header
                // deliberately doesn't include - kModSources/kModDests above
                // are this file's own copy of that same ordering.
                if (s == 2 && d == 6)
                {
                    L.patchBay.ampEnvAlwaysOn = { x, y, kModSources[s].accent, "ON" };
                    continue;
                }
                L.knobs.push_back({ modMatrixParam((uint32_t)s, (uint32_t)d), x, y, 10.0f, "", kModSources[s].accent });
            }
        }
    }

    // --- bottom bar ---
    {
        const PanelBox& p = L.panels[9];
        const float cy = p.y + p.h * 0.5f;
        L.knobs.push_back({ kParamMasterVolume, p.x + 50.0f, cy, 22.0f, "VOLUME", kCrtGreen });

        Selector mode;
        mode.param = kParamVoiceMode;
        mode.accent = p.accent;
        mode.x = p.x + 150.0f; mode.y = cy - 12.0f; mode.w = 120.0f; mode.h = 24.0f;
        mode.options = { { 0.0f, "POLY" }, { 1.0f, "MONO" } };
        L.selectors.push_back(mode);

        L.knobs.push_back({ kParamPortamentoTime, p.x + 400.0f, cy, 16.0f, "GLIDE", kCrtGreen });
        L.knobs.push_back({ kParamPitchBendRange, p.x + 450.0f, cy, 16.0f, "BEND", kCrtGreen });

        Selector glideMode;
        glideMode.param = kParamGlideMode;
        glideMode.accent = p.accent;
        glideMode.caption = "GLIDE MODE";
        glideMode.x = p.x + 500.0f; glideMode.y = cy - 12.0f; glideMode.w = 120.0f; glideMode.h = 24.0f;
        glideMode.options = { { 0.0f, "LEGATO" }, { 1.0f, "ALWAYS" } };
        L.selectors.push_back(glideMode);
    }

    return L;
}

// -----------------------------------------------------------------------------------------------------------
// value <-> knob-rotation mapping (shared by drawing and mouse-drag hit logic)

inline float paramToNormalized(uint32_t param, float value) noexcept
{
    const ParamInfo& info = getParamInfo(param);
    float t;
    if (info.shape == ParamShape::Logarithmic)
    {
        const float lo = info.min > 0.0f ? info.min : 1e-6f;
        const float hi = info.max > lo ? info.max : lo * 1.0001f;
        const float v = value < lo ? lo : (value > hi ? hi : value);
        t = (float)(std::log(v / lo) / std::log(hi / lo));
    }
    else
    {
        t = (value - info.min) / (info.max - info.min);
    }
    return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

inline float normalizedToParam(uint32_t param, float t) noexcept
{
    const ParamInfo& info = getParamInfo(param);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    if (info.shape == ParamShape::Logarithmic)
    {
        const float lo = info.min > 0.0f ? info.min : 1e-6f;
        const float hi = info.max > lo ? info.max : lo * 1.0001f;
        return lo * std::pow(hi / lo, t);
    }
    return info.min + (info.max - info.min) * t;
}

// Shift+drag snap increment for a knob, in raw param units (0 = no
// snapping for that param). Only assigned where a fixed step is actually
// musically meaningful - e.g. Ring/Sync's Ratio knob, where landing exactly
// on 0.5/1.0/1.5/2.0/... etc. matters far more than smooth sweeping does.
inline float shiftSnapStep(uint32_t param) noexcept
{
    switch (param)
    {
    case kParamRingSyncRatio: return 0.25f;
    default:                  return 0.0f;
    }
}

// selector option rects are needed both for drawing and for mouse hit-testing;
// compute them in one place so the two can't drift apart.
inline void getSelectorOptionRect(const Selector& sel, size_t i, double& ox, double& ow) noexcept
{
    const size_t n = sel.options.size();
    const double gap = 6.0;
    ow = (sel.w - gap * (double)(n - 1)) / (double)n;
    ox = sel.x + (ow + gap) * (double)i;
}

// dropdown open-list option rects, same drawing/hit-test-sharing rationale.
// opens downward normally, but flips upward if the list (which can be long -
// the tempo-sync menu has 15 entries) would otherwise run off the bottom of
// the window and there's more room above the closed box than below it.
inline void getDropdownOptionRect(const Dropdown& dd, size_t i, float layoutHeight, double& oy, double& oh) noexcept
{
    const double n = (double)dd.options.size();
    oh = dd.h - 4.0;

    const double downSpace = (double)layoutHeight - (dd.y + dd.h);
    const double upSpace = dd.y;
    const bool openUpward = (n * oh > downSpace) && (upSpace > downSpace);

    if (openUpward)
        oy = (dd.y - n * oh) + oh * (double)i;
    else
        oy = dd.y + dd.h + oh * (double)i;
}

// -----------------------------------------------------------------------------------------------------------
// value formatting

inline void formatParamValue(uint32_t index, float value, char* out, size_t outSize) noexcept
{
    switch (index)
    {
    case kParamWaveform:
        std::snprintf(out, outSize, "%s", value < 0.5f ? "SAW" : value < 1.5f ? "PULSE" : "TRI");
        return;
    case kParamEngineMode:
        std::snprintf(out, outSize, "%s", value < 0.5f ? "RING" : value < 1.5f ? "SYNC" : "COMBINED");
        return;
    case kParamRingSyncRatio:
        std::snprintf(out, outSize, "x%.2f", value);
        return;
    default:
        break;
    }

    const ParamInfo& info = getParamInfo(index);

    if (std::strcmp(info.unit, "Hz") == 0)
    {
        if (value >= 1000.0f)
            std::snprintf(out, outSize, "%.2fk", value / 1000.0);
        else
            std::snprintf(out, outSize, "%.0f", value);
    }
    else if (std::strcmp(info.unit, "s") == 0)
    {
        if (value < 1.0f)
            std::snprintf(out, outSize, "%.0fms", value * 1000.0);
        else
            std::snprintf(out, outSize, "%.2fs", value);
    }
    else if (std::strcmp(info.unit, "%") == 0)
    {
        std::snprintf(out, outSize, "%.0f%%", value);
    }
    else if (std::strcmp(info.unit, "st") == 0)
    {
        std::snprintf(out, outSize, "\xc2\xb1%dst", (int)(value + 0.5f)); // U+00B1 PLUS-MINUS SIGN
    }
    else
    {
        std::snprintf(out, outSize, "%.2f", value);
    }
}

// -----------------------------------------------------------------------------------------------------------
// drawing helpers

inline void roundedRect(cairo_t* cr, double x, double y, double w, double h, double r)
{
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r,     r, -M_PI_2, 0.0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0.0,      M_PI_2);
    cairo_arc(cr, x + r,     y + h - r, r, M_PI_2,   M_PI);
    cairo_arc(cr, x + r,     y + r,     r, M_PI,     3.0 * M_PI_2);
    cairo_close_path(cr);
}

// cairo_show_text() leaves a dangling current point; if the next drawing call
// is cairo_arc()/cairo_line_to() without an explicit new subpath, cairo will
// implicitly connect from that stray point. Always reset the path after text.
inline void drawText(cairo_t* cr, const char* text, double x, double y)
{
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);
    cairo_new_path(cr);
}

inline void centeredText(cairo_t* cr, const char* text, double cx, double cy)
{
    cairo_text_extents_t ext;
    cairo_text_extents(cr, text, &ext);
    drawText(cr, text, cx - ext.width / 2.0 - ext.x_bearing, cy - ext.height / 2.0 - ext.y_bearing);
}

inline void setFont(cairo_t* cr, double size, bool bold = true)
{
    cairo_select_font_face(cr, "monospace",
                            CAIRO_FONT_SLANT_NORMAL,
                            bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
}

// -----------------------------------------------------------------------------------------------------------
// paint

struct PaintState
{
    const float* values = nullptr;   // [kParamCount]
    int hoverKnob = -1;               // index into layout.knobs, or -1
    int dragKnob = -1;
    int hoverSelector = -1;           // index into layout.selectors, or -1
    int hoverSelectorOption = -1;
    int hoverDropdown = -1;            // index into layout.dropdowns, or -1 (closed-box hover)
    int openDropdown = -1;            // index into layout.dropdowns, or -1
    int hoverDropdownOption = -1;
    const bool* autoShowValue = nullptr; // [kParamCount], true = briefly show this
                                          // knob's value even without dragging
                                          // (e.g. just changed via automation)

    // preset bar - pure UI-side state, not backed by any DPF parameter (see
    // ui/PresetStore.hpp)
    const char* presetName = "INIT";
    bool presetIsUnsaved = false; // true when a param has changed since the
                                   // displayed preset was last loaded/saved
    bool presetEditingName = false;
    const char* presetEditBuffer = "";
    int hoverPresetButton = -1;   // 0=prev,1=next,2=save,3=delete, or -1
    bool presetDeleteEnabled = false; // false while on INIT - nothing to delete
};

inline void paintKnob(cairo_t* cr, const Knob& k, float value, bool hovered, bool dragging, bool autoShow)
{
    const float t = paramToNormalized(k.param, value);

    const double startAngle = M_PI * 0.75;   // 135deg
    const double sweep = M_PI * 1.5;         // 270deg total
    const double valueAngle = startAngle + sweep * t;

    // outer ring track
    cairo_set_line_width(cr, 5.0);
    setColor(cr, kKnobRing);
    cairo_arc(cr, k.cx, k.cy, k.radius, startAngle, startAngle + sweep);
    cairo_stroke(cr);

    // value arc
    setColor(cr, k.accent, dragging ? 1.0 : (hovered ? 0.9 : 0.75));
    cairo_arc(cr, k.cx, k.cy, k.radius, startAngle, valueAngle);
    cairo_stroke(cr);

    // body
    setColor(cr, kKnobBody);
    cairo_arc(cr, k.cx, k.cy, k.radius - 6.0, 0.0, 2.0 * M_PI);
    cairo_fill(cr);

    if (hovered || dragging)
    {
        setColor(cr, k.accent, 0.18);
        cairo_arc(cr, k.cx, k.cy, k.radius - 6.0, 0.0, 2.0 * M_PI);
        cairo_fill(cr);
    }

    // pointer
    setColor(cr, k.accent);
    cairo_set_line_width(cr, 3.0);
    const double px = k.cx + std::cos(valueAngle) * (k.radius - 10.0);
    const double py = k.cy + std::sin(valueAngle) * (k.radius - 10.0);
    cairo_move_to(cr, k.cx, k.cy);
    cairo_line_to(cr, px, py);
    cairo_stroke(cr);

    // label
    if (k.label[0] != '\0')
    {
        setFont(cr, 10.5);
        setColor(cr, kTextDim);
        centeredText(cr, k.label, k.cx, k.cy + k.radius + 14.0);
    }

    if (dragging || autoShow)
    {
        char buf[32];
        formatParamValue(k.param, value, buf, sizeof(buf));
        setFont(cr, 11.5);
        setColor(cr, k.accent);
        centeredText(cr, buf, k.cx, k.cy - k.radius - 12.0);
    }
}

// mirrors ADSR::setCurve()'s shaping exactly, so the preview matches what's
// actually heard: exponent 1 (curve=0, linear) down to 0.25 (curve=1,
// fast-move/slow-settle analog feel)
inline float envelopeCurveShape(float t, float curveAmount) noexcept
{
    const float exponent = std::pow(4.0f, -curveAmount);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return std::pow(t, exponent);
}

// read-only ADSR shape preview. Segment widths are fixed proportions of the
// graph (not literal time), since attack/decay/release can differ by
// several orders of magnitude - a literal time scale would make short
// stages invisible next to a multi-second release. The curve shaping is
// still drawn exactly, which is the musically relevant part to see anyway.
inline void paintEnvelopeGraph(cairo_t* cr, const EnvelopeGraph& eg, const float* values)
{
    const float sustain = values[eg.sustainParam];
    const float curve = values[eg.curveParam];

    const float attackFrac = 0.25f, decayFrac = 0.25f, holdFrac = 0.20f, releaseFrac = 0.30f;

    roundedRect(cr, eg.x, eg.y, eg.w, eg.h, 4.0);
    setColor(cr, kBg, 0.55);
    cairo_fill(cr);

    const int steps = 14;
    const double x0 = eg.x, y0 = eg.y, w = eg.w, h = eg.h;
    auto plotX = [&](float frac) { return x0 + w * (double)frac; };
    auto plotY = [&](float level) { return y0 + h * (1.0 - (double)level); };

    cairo_new_path(cr);
    cairo_move_to(cr, plotX(0.0f), plotY(0.0f));

    for (int i = 1; i <= steps; ++i)
    {
        const float t = (float)i / (float)steps;
        const float level = envelopeCurveShape(t, curve);
        cairo_line_to(cr, plotX(attackFrac * t), plotY(level));
    }
    for (int i = 1; i <= steps; ++i)
    {
        const float t = (float)i / (float)steps;
        const float shaped = envelopeCurveShape(t, curve);
        const float level = 1.0f + (sustain - 1.0f) * shaped;
        cairo_line_to(cr, plotX(attackFrac + decayFrac * t), plotY(level));
    }
    cairo_line_to(cr, plotX(attackFrac + decayFrac + holdFrac), plotY(sustain));
    for (int i = 1; i <= steps; ++i)
    {
        const float t = (float)i / (float)steps;
        const float shaped = envelopeCurveShape(t, curve);
        const float level = sustain * (1.0f - shaped);
        cairo_line_to(cr, plotX(attackFrac + decayFrac + holdFrac + releaseFrac * t), plotY(level));
    }

    setColor(cr, eg.accent, 0.9);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke_preserve(cr);

    cairo_line_to(cr, plotX(1.0f), plotY(0.0f));
    cairo_line_to(cr, plotX(0.0f), plotY(0.0f));
    cairo_close_path(cr);
    setColor(cr, eg.accent, 0.16);
    cairo_fill(cr);
}

inline void paintSelector(cairo_t* cr, const Selector& sel, float value, int hoverOption)
{
    if (sel.caption != nullptr)
    {
        setFont(cr, 9.0, false);
        setColor(cr, kTextDim);
        drawText(cr, sel.caption, sel.x, sel.y - 6.0);
    }

    const size_t n = sel.options.size();

    for (size_t i = 0; i < n; ++i)
    {
        double ox, optW;
        getSelectorOptionRect(sel, i, ox, optW);
        const bool active = std::fabs(sel.options[i].value - value) < 0.001f;
        const bool hovered = (int)i == hoverOption;

        roundedRect(cr, ox, sel.y, optW, sel.h, 4.0);
        if (active)
            setColor(cr, sel.accent, 0.9);
        else
            setColor(cr, kBtnBg, hovered ? 1.0 : 0.85);
        cairo_fill_preserve(cr);
        setColor(cr, active ? sel.accent : kPanelEdge);
        cairo_set_line_width(cr, 1.5);
        cairo_stroke(cr);

        setFont(cr, 11.0);
        setColor(cr, active ? kBg : kTextMain);
        centeredText(cr, sel.options[i].label, ox + optW / 2.0, sel.y + sel.h / 2.0);
    }
}

inline const char* findOptionLabel(const std::vector<SelectorOption>& options, float value) noexcept
{
    for (const SelectorOption& opt : options)
        if (std::fabs(opt.value - value) < 0.001f)
            return opt.label;
    return "?";
}

inline void paintDropdownClosed(cairo_t* cr, const Dropdown& dd, float value, bool hovered, bool open)
{
    roundedRect(cr, dd.x, dd.y, dd.w, dd.h, 4.0);
    setColor(cr, kBtnBg, (hovered || open) ? 1.0 : 0.85);
    cairo_fill_preserve(cr);
    setColor(cr, open ? dd.accent : kPanelEdge, 1.0);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    setFont(cr, 11.0);
    setColor(cr, dd.accent);
    cairo_text_extents_t ext;
    const char* label = findOptionLabel(dd.options, value);
    cairo_text_extents(cr, label, &ext);
    drawText(cr, label, dd.x + 12.0, dd.y + dd.h / 2.0 - ext.height / 2.0 - ext.y_bearing);

    // little down/up arrow on the right to signal "combo box"
    const double ax = dd.x + dd.w - 20.0, ay = dd.y + dd.h / 2.0;
    setColor(cr, kTextDim);
    cairo_move_to(cr, ax - 5.0, open ? ay + 3.0 : ay - 3.0);
    cairo_line_to(cr, ax + 5.0, open ? ay + 3.0 : ay - 3.0);
    cairo_line_to(cr, ax,       open ? ay - 3.0 : ay + 3.0);
    cairo_close_path(cr);
    cairo_fill(cr);
}

inline void paintDropdownOpenList(cairo_t* cr, const Dropdown& dd, float value, int hoverOption, float layoutHeight)
{
    const size_t n = dd.options.size();
    double oyFirst, oyLast, oh;
    getDropdownOptionRect(dd, 0, layoutHeight, oyFirst, oh);
    getDropdownOptionRect(dd, n - 1, layoutHeight, oyLast, oh);
    const double listTop = oyFirst < oyLast ? oyFirst : oyLast;
    const double listBottom = (oyFirst < oyLast ? oyLast : oyFirst) + oh;

    // backdrop so the list reads clearly over whatever is beneath it
    setColor(cr, kBg, 0.92);
    cairo_rectangle(cr, dd.x, listTop, dd.w, listBottom - listTop);
    cairo_fill(cr);

    for (size_t i = 0; i < n; ++i)
    {
        double oy;
        getDropdownOptionRect(dd, i, layoutHeight, oy, oh);
        const bool active = std::fabs(dd.options[i].value - value) < 0.001f;
        const bool hovered = (int)i == hoverOption;

        cairo_rectangle(cr, dd.x, oy, dd.w, oh);
        if (active)
            setColor(cr, dd.accent, 0.9);
        else
            setColor(cr, kBtnBg, hovered ? 1.0 : 0.0);
        cairo_fill(cr);

        setFont(cr, 10.5);
        setColor(cr, active ? kBg : kTextMain);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, dd.options[i].label, &ext);
        drawText(cr, dd.options[i].label, dd.x + 12.0, oy + oh / 2.0 - ext.height / 2.0 - ext.y_bearing);
    }

    setColor(cr, dd.accent, 0.8);
    cairo_set_line_width(cr, 1.5);
    cairo_rectangle(cr, dd.x, listTop, dd.w, listBottom - listTop);
    cairo_stroke(cr);
}

// patch bay jack sockets - sources across a header row along the top,
// destinations down the left edge (see the "patch bay" comment in
// buildLayout() for why the grid is transposed this way) - always drawn,
// regardless of connection state.
inline void paintPatchJacks(cairo_t* cr, const PatchBayLayout& pb)
{
    for (int i = 0; i < kModSourceCount; ++i)
    {
        const ModJack& j = pb.sources[i];
        setColor(cr, kBg);
        cairo_arc(cr, j.x, j.y, 6.0, 0.0, 2.0 * M_PI);
        cairo_fill(cr);
        setColor(cr, j.accent, 0.95);
        cairo_set_line_width(cr, 2.0);
        cairo_arc(cr, j.x, j.y, 6.0, 0.0, 2.0 * M_PI);
        cairo_stroke(cr);

        setFont(cr, 8.0);
        setColor(cr, j.accent);
        centeredText(cr, j.label, j.x, j.y - 15.0);
    }

    for (int i = 0; i < kModDestCount; ++i)
    {
        const ModJack& j = pb.dests[i];
        setColor(cr, kBg);
        cairo_arc(cr, j.x, j.y, 5.5, 0.0, 2.0 * M_PI);
        cairo_fill(cr);
        setColor(cr, j.accent, 0.9);
        cairo_set_line_width(cr, 2.0);
        cairo_arc(cr, j.x, j.y, 5.5, 0.0, 2.0 * M_PI);
        cairo_stroke(cr);

        setFont(cr, 9.5, false);
        setColor(cr, kTextDim);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, j.label, &ext);
        drawText(cr, j.label, j.x + 12.0, j.y - ext.height / 2.0 - ext.y_bearing);
    }

    // Amp Env -> Amplitude: always-on, not a patchable knob - drawn as a
    // permanently solid-filled dot (vs. every other cell's knob-with-a-
    // pointer look) so it visually reads as fixed rather than "off"
    {
        const ModJack& j = pb.ampEnvAlwaysOn;
        setColor(cr, j.accent, 0.9);
        cairo_arc(cr, j.x, j.y, 7.0, 0.0, 2.0 * M_PI);
        cairo_fill(cr);
        setColor(cr, kBg);
        cairo_arc(cr, j.x, j.y, 7.0, 0.0, 2.0 * M_PI);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        setFont(cr, 7.0, false);
        setColor(cr, kBg);
        centeredText(cr, j.label, j.x, j.y + 0.5);
    }
}

// active patch cables: one cable per (source, dest) cell whose current
// depth is non-zero, colored by its source, drawn as a direct point-to-point
// S-curve bezier from the source jack to the destination jack (standard
// node-editor connector shape). Deliberately *not* routed along the grid's
// rows/columns - an axis-aligned route from a source through its whole row
// to a far column visually passes directly over every unconnected cell in
// between, which reads as "also connected" even though it isn't. A direct
// diagonal only reads as connecting its own two endpoints, no matter how
// many other cells it happens to cross on the way.
inline void paintPatchCables(cairo_t* cr, const PatchBayLayout& pb, const float* values)
{
    for (int s = 0; s < kModSourceCount; ++s)
    {
        const ModJack& src = pb.sources[s];
        for (int d = 0; d < kModDestCount; ++d)
        {
            const float depth = values[modMatrixParam((uint32_t)s, (uint32_t)d)];
            float mag = depth < 0.0f ? -depth : depth;
            if (mag < 0.02f)
                continue;
            mag = mag > 1.0f ? 1.0f : mag;

            const ModJack& dst = pb.dests[d];
            const double alpha = 0.35 + 0.55 * (double)mag;
            const double lineWidth = 1.3 + 2.0 * (double)mag;
            const double dx = dst.x - src.x;
            const double dy = dst.y - src.y;

            setColor(cr, src.accent, alpha);
            cairo_set_line_width(cr, lineWidth);
            cairo_move_to(cr, src.x, src.y);
            cairo_curve_to(cr, src.x + dx * 0.25, src.y + dy * 0.75,
                               dst.x - dx * 0.25, dst.y - dy * 0.75,
                               dst.x, dst.y);
            cairo_stroke(cr);
        }
    }
}

inline void paintButton(cairo_t* cr, const Button& b, bool hovered, bool enabled)
{
    roundedRect(cr, b.x, b.y, b.w, b.h, 4.0);
    setColor(cr, kBtnBg, enabled ? (hovered ? 1.0 : 0.85) : 0.4);
    cairo_fill_preserve(cr);
    setColor(cr, b.accent, enabled ? 1.0 : 0.35);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    setFont(cr, 11.5);
    setColor(cr, b.accent, enabled ? 1.0 : 0.35);
    centeredText(cr, b.label, b.x + b.w / 2.0, b.y + b.h / 2.0);
}

inline void paintPresetBar(cairo_t* cr, const PresetBarLayout& pb, const PaintState& state)
{
    paintButton(cr, pb.prev, state.hoverPresetButton == 0, true);
    paintButton(cr, pb.next, state.hoverPresetButton == 1, true);
    paintButton(cr, pb.save, state.hoverPresetButton == 2, true);
    paintButton(cr, pb.del,  state.hoverPresetButton == 3, state.presetDeleteEnabled);

    roundedRect(cr, pb.nameX, pb.nameY, pb.nameW, pb.nameH, 4.0);
    setColor(cr, state.presetEditingName ? kKnobBody : kBtnBg, 0.9);
    cairo_fill_preserve(cr);
    setColor(cr, state.presetEditingName ? pb.accent : kPanelEdge, state.presetEditingName ? 1.0 : 0.8);
    cairo_set_line_width(cr, state.presetEditingName ? 2.0 : 1.5);
    cairo_stroke(cr);

    setFont(cr, 9.0, false);
    setColor(cr, kTextDim);
    drawText(cr, "PRESET", pb.nameX + 12.0, pb.nameY + 14.0);

    setFont(cr, 13.0);
    if (state.presetEditingName)
    {
        char buf[40];
        std::snprintf(buf, sizeof(buf), "%s_", state.presetEditBuffer);
        setColor(cr, kTextMain);
        drawText(cr, buf, pb.nameX + 12.0, pb.nameY + pb.nameH - 12.0);
    }
    else
    {
        setColor(cr, state.presetIsUnsaved ? kAmber : kTextMain);
        char buf[40];
        std::snprintf(buf, sizeof(buf), "%s%s", state.presetName, state.presetIsUnsaved ? " *" : "");
        drawText(cr, buf, pb.nameX + 12.0, pb.nameY + pb.nameH - 12.0);
    }
}

inline void paint(cairo_t* cr, const Layout& L, const PaintState& state)
{
    // background
    setColor(cr, kBg);
    cairo_paint(cr);

    // header
    setFont(cr, 26.0);
    setColor(cr, kCrtGreen);
    drawText(cr, "SIDEOUS BASS", 20.0, 40.0);

    setFont(cr, 10.0, false);
    setColor(cr, kTextDim);
    {
        const char* subtitle = "8-BIT BASS SPECIALIST";
        cairo_text_extents_t ext;
        cairo_text_extents(cr, subtitle, &ext);
        drawText(cr, subtitle, L.width - 20.0 - ext.width, 24.0);
    }

    paintPresetBar(cr, L.presetBar, state);

    // panels
    for (const PanelBox& p : L.panels)
    {
        roundedRect(cr, p.x, p.y, p.w, p.h, 8.0);
        setColor(cr, kPanelBg);
        cairo_fill_preserve(cr);
        setColor(cr, kPanelEdge, 0.8);
        cairo_set_line_width(cr, 1.5);
        cairo_stroke(cr);

        if (p.title != nullptr)
        {
            setFont(cr, 12.0);
            setColor(cr, p.accent);
            drawText(cr, p.title, p.x + 14.0, p.y + 22.0);
        }
    }

    // envelope graphs (read-only, drawn under the knobs)
    for (const EnvelopeGraph& eg : L.envelopeGraphs)
        paintEnvelopeGraph(cr, eg, state.values);

    // patch bay jacks + cables, drawn under the (tiny, per-cell) knobs so the
    // knobs remain the topmost, clickable layer at each connection point
    // cables drawn *before* jacks so a cable passing near a jack's label
    // never overlaps/obscures the text
    paintPatchCables(cr, L.patchBay, state.values);
    paintPatchJacks(cr, L.patchBay);

    // selectors
    for (size_t i = 0; i < L.selectors.size(); ++i)
    {
        const Selector& sel = L.selectors[i];
        const int hoverOption = (int)i == state.hoverSelector ? state.hoverSelectorOption : -1;
        paintSelector(cr, sel, state.values[sel.param], hoverOption);
    }

    // dropdowns (closed boxes only here; the open list, if any, must draw
    // after the knobs below so it visually overlays them)
    for (size_t i = 0; i < L.dropdowns.size(); ++i)
    {
        const Dropdown& dd = L.dropdowns[i];
        paintDropdownClosed(cr, dd, state.values[dd.param], (int)i == state.hoverDropdown, (int)i == state.openDropdown);
    }

    // knobs
    for (size_t i = 0; i < L.knobs.size(); ++i)
    {
        const Knob& k = L.knobs[i];
        const bool autoShow = state.autoShowValue != nullptr && state.autoShowValue[k.param];
        paintKnob(cr, k, state.values[k.param], (int)i == state.hoverKnob, (int)i == state.dragKnob, autoShow);
    }

    // open dropdown list, drawn last so it overlays whatever is beneath it
    if (state.openDropdown >= 0 && (size_t)state.openDropdown < L.dropdowns.size())
    {
        const Dropdown& dd = L.dropdowns[state.openDropdown];
        paintDropdownOpenList(cr, dd, state.values[dd.param], state.hoverDropdownOption, L.height);
    }
}

} // namespace ui
} // namespace sideous
