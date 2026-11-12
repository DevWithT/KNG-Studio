#include "KngFattenerPlugin.h"

namespace te = tracktion;

const char* KngFattenerPlugin::xmlTypeName = "kngfattener";

static std::vector<KngParamDef> fattenerParams()
{
    return {
        { "sub",      "Sub",        0.0f,  1.0f, 0.3f,  0.0f, 0.0f, "%",  nullptr },
        { "octave",   "Octave",     0.0f,  1.0f, 0.0f,  0.0f, 0.0f, "%",  nullptr },
        { "punch",    "Punch",      0.0f,  1.0f, 0.3f,  0.0f, 0.0f, "%",  nullptr },
        { "drive",    "Drive",      0.0f,  1.0f, 0.25f, 0.0f, 0.0f, "%",  nullptr },
        { "tone",     "Tone",      -1.0f,  1.0f, 0.0f,  0.0f, 0.0f, "%",  nullptr },
        { "clip",     "Clip",       0.0f,  1.0f, 0.0f,  0.0f, 0.0f, "%",  nullptr },
        { "monoLows", "Mono Lows",  0.0f,  1.0f, 1.0f,  0.0f, 1.0f, "",   "toggle" },
        { "output",   "Output",   -12.0f, 12.0f, 0.0f,  0.0f, 0.0f, "dB", nullptr },
        { "mix",      "Mix",        0.0f,  1.0f, 1.0f,  0.0f, 0.0f, "%",  nullptr }
    };
}

static std::vector<KngPresetDef> fattenerPresets()
{
    using P = KngFattenerPlugin;

    return {
        { "Default", {} },
        { "Thick 808",         { { P::sub, 0.5f }, { P::punch, 0.2f }, { P::drive, 0.3f } } },
        { "Hard Kick",         { { P::sub, 0.2f }, { P::punch, 0.8f }, { P::drive, 0.35f }, { P::tone, 0.25f }, { P::clip, 0.4f } } },
        { "Distorted 808",     { { P::sub, 0.3f }, { P::punch, 0.4f }, { P::drive, 0.85f }, { P::tone, 0.3f }, { P::clip, 0.5f }, { P::output, -3.0f } } },
        { "Club Sub",          { { P::sub, 0.8f }, { P::octave, 0.3f }, { P::punch, 0.1f }, { P::drive, 0.1f }, { P::tone, -0.4f } } },
        { "Phone Speaker 808", { { P::sub, 0.2f }, { P::drive, 0.6f }, { P::tone, 0.5f }, { P::clip, 0.3f } } }
    };
}

// Drive: warm at low settings, dirty at the top
static float saturate (float x, float drive)
{
    if (drive < 0.001f)
        return x;

    const float g = 1.0f + drive * 6.0f;
    return std::tanh (x * g) / std::tanh (g) / (1.0f + drive);
}

// Clip: push it in, round off anything above 0.8, never past 0.95
static float clipSample (float x, float preGain)
{
    x *= preGain;
    const float a = std::abs (x);

    if (a <= 0.8f)
        return x;

    return std::copysign (0.8f + 0.15f * std::tanh ((a - 0.8f) / 0.15f), x);
}

KngFattenerPlugin::KngFattenerPlugin (te::PluginCreationInfo creationInfo)
    : KngFxBase (creationInfo, fattenerParams(), fattenerPresets())
{
}

void KngFattenerPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;
    reset();
}

void KngFattenerPlugin::reset()
{
    for (auto* arr : { &subShelf, &toneShelf, &monoSplit })
        for (auto& f : *arr)
            f.reset();

    for (auto* f : { &octIn1, &octIn2, &octOut1, &octOut2 })
        f->reset();

    fastEnv.reset();
    slowEnv.reset();
    octEnv.reset();
    flip = 1.0f;
    above = false;
}

void KngFattenerPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
        return;

    juce::ScopedNoDenormals noDenormals;

    const double sr = sampleRate;
    const float subAmt = get (sub), octAmt = get (octave), punchAmt = get (punch), driveAmt = get (drive);
    const float toneAmt = get (tone), clipAmt = get (clip), mixAmt = get (mix);
    const bool monoOn = get (monoLows) > 0.5f;
    const float outGain = juce::Decibels::decibelsToGain (get (output));
    const float clipGain = juce::Decibels::decibelsToGain (clipAmt * 12.0f);

    // Filters for this block
    for (size_t ch = 0; ch < 2; ++ch)
    {
        subShelf[ch].lowShelf (sr, 90.0, subAmt * 9.0);
        toneShelf[ch].highShelf (sr, 1800.0, toneAmt * 9.0);
        monoSplit[ch].lowPass (sr, 120.0);
    }

    octIn1.lowPass (sr, 150.0);
    octIn2.lowPass (sr, 150.0);
    octOut1.lowPass (sr, 90.0);
    octOut2.lowPass (sr, 90.0);

    const float fastA = KngDsp::Follower::coef (1.0, sr),  fastR = KngDsp::Follower::coef (30.0, sr);
    const float slowA = KngDsp::Follower::coef (25.0, sr), slowR = KngDsp::Follower::coef (250.0, sr);
    const float octA  = KngDsp::Follower::coef (2.0, sr),  octR  = KngDsp::Follower::coef (60.0, sr);

    auto& buffer = *fc.destBuffer;
    auto* left  = buffer.getWritePointer (0, fc.bufferStartSample);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr;

    float inMax = 0.0f, outMax = 0.0f;

    for (int i = 0; i < fc.bufferNumSamples; ++i)
    {
        const float dryL = left[i];
        const float dryR = right != nullptr ? right[i] : dryL;
        const float m = 0.5f * (dryL + dryR);
        float l = dryL, r = dryR;

        // Punch: lift the hit's attack above its body
        if (punchAmt > 0.001f)
        {
            const float fast = fastEnv.process (m, fastA, fastR);
            const float slow = slowEnv.process (m, slowA, slowR);
            const float g = std::pow (juce::jlimit (1.0f, 4.0f, (fast + 1.0e-5f) / (slow + 1.0e-5f)), punchAmt * 0.8f);
            l *= g;
            r *= g;
        }

        // Sub: more weight under 90 Hz
        l = subShelf[0].process (l);
        r = subShelf[1].process (r);

        // Octave: a smooth layer one octave under the low end
        if (octAmt > 0.001f)
        {
            const float low = octIn2.process (octIn1.process (m));

            if (! above && low > 0.002f)
            {
                above = true;
                flip = -flip;
            }
            else if (above && low < -0.002f)
            {
                above = false;
            }

            const float env = octEnv.process (low, octA, octR);
            const float layer = octOut2.process (octOut1.process (flip * env)) * octAmt * 1.5f;
            l += layer;
            r += layer;
        }

        // Drive, then tone
        l = toneShelf[0].process (saturate (l, driveAmt));
        r = toneShelf[1].process (saturate (r, driveAmt));

        // Mono lows: everything under 120 Hz sits in the center
        if (monoOn)
        {
            const float lowL = monoSplit[0].process (l);
            const float lowR = monoSplit[1].process (r);
            const float lowMid = 0.5f * (lowL + lowR);
            l += lowMid - lowL;
            r += lowMid - lowR;
        }

        // Clip: louder without going over
        if (clipAmt > 0.001f)
        {
            l = clipSample (l, clipGain);
            r = clipSample (r, clipGain);
        }

        l = (dryL + (l - dryL) * mixAmt) * outGain;
        r = (dryR + (r - dryR) * mixAmt) * outGain;

        left[i] = l;

        if (right != nullptr)
            right[i] = r;

        inMax = juce::jmax (inMax, std::abs (dryL), std::abs (dryR));
        outMax = juce::jmax (outMax, std::abs (l), std::abs (r));
    }

    reportMeters (inMax, outMax, 0.0f);
}

void KngFattenerPlugin::drawDisplay (juce::Graphics& g, juce::Rectangle<float> area, const KngHistory&)
{
    g.setColour (juce::Colour (0xff141618));
    g.fillRoundedRectangle (area, 6.0f);

    // The drive and clip curve: input left to right, output bottom to top
    auto plot = area.reduced (16.0f).withTrimmedTop (16.0f);
    plot = plot.withSizeKeepingCentre (juce::jmin (plot.getWidth(), plot.getHeight() * 1.6f), plot.getHeight());

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawLine (plot.getX(), plot.getCentreY(), plot.getRight(), plot.getCentreY());
    g.drawLine (plot.getCentreX(), plot.getY(), plot.getCentreX(), plot.getBottom());
    g.drawLine (plot.getX(), plot.getBottom(), plot.getRight(), plot.getY());

    const float driveAmt = get (drive), clipAmt = get (clip);
    const float clipGain = juce::Decibels::decibelsToGain (clipAmt * 12.0f);
    juce::Path curve;

    for (int k = 0; k <= 200; ++k)
    {
        const float x = -1.0f + 2.0f * (float) k / 200.0f;
        float y = saturate (x, driveAmt);

        if (clipAmt > 0.001f)
            y = clipSample (y, clipGain);

        y = juce::jlimit (-1.0f, 1.0f, y);

        const float px = plot.getX() + (x + 1.0f) * 0.5f * plot.getWidth();
        const float py = plot.getBottom() - (y + 1.0f) * 0.5f * plot.getHeight();

        if (k == 0)
            curve.startNewSubPath (px, py);
        else
            curve.lineTo (px, py);
    }

    g.setColour (juce::Colour (0xffe6823a));
    g.strokePath (curve, juce::PathStrokeType (2.5f));

    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("Sub +" + juce::String (get (sub) * 9.0f, 1) + " dB    Octave "
                    + juce::String (juce::roundToInt (get (octave) * 100.0f)) + "%    Punch "
                    + juce::String (juce::roundToInt (get (punch) * 100.0f)) + "%"
                    + (get (monoLows) > 0.5f ? "    Mono lows" : ""),
                area.reduced (12.0f).toNearestInt(), juce::Justification::topLeft);
}
