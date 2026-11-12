#include "InspectorCards.h"
#include "InspectorParts.h"
#include "Kng808Plugin.h"
#include "KngSynthPlugin.h"
#include "KngSamplerPlugin.h"
#include "KngPopOut.h"
#include "TheoryHelper.h"

namespace te = tracktion;

//==============================================================================
KngIdentity kngIdentityFor (te::Plugin& p)
{
    auto make = [] (juce::uint32 accent, juce::uint32 top, juce::uint32 bottom, const juce::String& chip,
                    KngFonts::Face chipFace, juce::uint32 light, juce::uint32 dark, juce::uint32 label,
                    KngFonts::Face labelFace, bool upper)
    {
        KngIdentity id;
        id.accent = juce::Colour (accent);
        id.top = juce::Colour (top);
        id.bottom = juce::Colour (bottom);
        id.chip = chip;
        id.chipFace = chipFace;

        auto& s = id.style;
        s.accent    = id.accent;
        s.bodyLight = juce::Colour (light);
        s.bodyDark  = juce::Colour (dark);
        s.pointer   = juce::Colours::white.withAlpha (0.92f);
        s.track     = juce::Colours::white.withAlpha (0.08f);
        s.label     = juce::Colour (label);
        s.value     = id.accent.brighter (0.3f);
        s.panel     = id.bottom.darker (0.3f);
        s.outline   = id.accent.withAlpha (0.35f);
        s.labelFace = labelFace;
        s.valueFace = KngFonts::mono;
        s.menuFace  = KngFonts::mono;
        s.upperCaseLabels = upper;
        return id;
    };

    const auto name = p.getName();

    if (name == "KNG 808")
        return make (0xffff7a1a, 0xff1d1e21, 0xff131416, "8", KngFonts::barlowHeavy, 0xff3b3d41, 0xff151619, 0xffa59c92, KngFonts::barlowSemi, true);

    if (name == "KNG Synth")
        return make (0xff3ee0ff, 0xff14213b, 0xff0b1220, "S", KngFonts::spaceGrotesk, 0xff24324d, 0xff0c1322, 0xff8aa0c8, KngFonts::spaceGrotesk, false);

    if (name == "KNG Sampler")
        return make (0xff9b7bff, 0xff1c1924, 0xff121018, "SP", KngFonts::sora, 0xff34303f, 0xff14121a, 0xff9a93b0, KngFonts::sora, false);

    if (name == "KNG Fattener")
        return make (0xffff3b30, 0xff3a1416, 0xff1c0a0b, "F", KngFonts::anton, 0xff4a2426, 0xff1a0d0e, 0xffc08a85, KngFonts::spaceGrotesk, true);

    // The other KNG effects share a dark card with their own color
    struct Effect { const char* name; juce::uint32 colour; const char* chip; };
    static const Effect effects[] = {
        { "KNG EQ", 0xff2fbf71, "EQ" },        { "KNG Comp", 0xffffb020, "C" },     { "KNG Verb", 0xff5b8cff, "V" },
        { "KNG Delay", 0xff22c3b5, "D" },      { "KNG Saturator", 0xffff7a45, "SA" }, { "KNG De-Esser", 0xffe879f9, "DS" },
        { "KNG Limiter", 0xfff43f5e, "L" },    { "KNG Doubler", 0xff38bdf8, "2x" }
    };

    for (auto& e : effects)
        if (name == e.name)
            return make (e.colour, 0xff1e2124, 0xff17191b, e.chip, KngFonts::spaceGrotesk, 0xff3a3f45, 0xff1a1c1f, 0xff9aa3ad, KngFonts::spaceGrotesk, true);

    // VSTs and anything else: grey, with their first letter
    return make (0xff607d8b, 0xff1e2124, 0xff17191b, name.substring (0, 1).toUpperCase(), KngFonts::spaceGrotesk,
                 0xff3a3f45, 0xff1a1c1f, 0xff9aa3ad, KngFonts::spaceGrotesk, true);
}

void kngDrawChip (juce::Graphics& g, juce::Rectangle<float> area, const KngIdentity& id, float alpha)
{
    g.setColour (id.accent.withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (area, 6.0f);

    const bool light = id.accent.getPerceivedBrightness() > 0.6f;
    g.setColour ((light ? juce::Colours::black.withAlpha (0.8f) : juce::Colours::white).withMultipliedAlpha (alpha));
    g.setFont (KngFonts::font (id.chipFace, area.getHeight() * (id.chip.length() > 1 ? 0.45f : 0.6f)));
    g.drawText (id.chip, area, juce::Justification::centred, false);
}

//==============================================================================
InstrumentCard::InstrumentCard (EditSession& sessionToUse, te::Plugin& instrument)
    : session (sessionToUse), plugin (&instrument), id (kngIdentityFor (instrument)), look (id.style)
{
    setLookAndFeel (&look);
    setTooltip ("Click to open the full panel");

    kind = dynamic_cast<Kng808Plugin*> (&instrument) != nullptr    ? eight08
         : dynamic_cast<KngSynthPlugin*> (&instrument) != nullptr  ? synth
         : dynamic_cast<KngSamplerPlugin*> (&instrument) != nullptr ? sampler
                                                                   : generic;

    addAndMakeVisible (changeButton);
    changeButton.setTooltip ("Swap this track's instrument");
    changeButton.onClick = [this] { if (onChange) onChange(); };

    addChildComponent (popButton);
    popButton.setVisible (instrument.getPluginType().startsWith ("kng"));
    popButton.setTooltip ("Open in its own window");
    popButton.onClick = [this] { KngPopOut::open (*plugin); };

    auto addFaceKnob = [this] (std::unique_ptr<KngFaceKnob> k, juce::Colour arc)
    {
        k->labelSize = 9.5f;
        k->valueSize = 9.0f;
        k->setArcColour (arc);
        addAndMakeVisible (*k);
        knobs.push_back (std::move (k));
    };

    if (auto* p808 = dynamic_cast<Kng808Plugin*> (&instrument))
    {
        addFaceKnob (std::make_unique<KngFaceKnob> (*p808, Kng808Plugin::drop,   id.style, "Drop"),   id.accent);
        addFaceKnob (std::make_unique<KngFaceKnob> (*p808, Kng808Plugin::length, id.style, "Length"), id.accent);
        addFaceKnob (std::make_unique<KngFaceKnob> (*p808, Kng808Plugin::punch,  id.style, "Punch"),  id.accent);
        addFaceKnob (std::make_unique<KngFaceKnob> (*p808, Kng808Plugin::drive,  id.style, "Drive"),  juce::Colour (0xffff4d1a));
    }
    else if (auto* syn = dynamic_cast<KngSynthPlugin*> (&instrument))
    {
        auto synthKnob = [&] (int param, const char* unit, const char* label, juce::Colour arc)
        {
            const auto& in = KngSynth::info (param);
            const KngParamDef def { in.id, in.name, in.min, in.max, in.def, in.mid, in.step, unit, in.choices };
            addFaceKnob (std::make_unique<KngFaceKnob> (instrument, *syn->getParam (param), def, id.style, label), arc);
        };

        const juce::Colour pink (0xffff5fa8);
        synthKnob (KngSynth::cutoff, "Hz",  "Cutoff",  pink);
        synthKnob (KngSynth::reso,   "%",   "Reso",    pink);
        synthKnob (KngSynth::ampA,   "sec", "Attack",  id.accent);
        synthKnob (KngSynth::ampR,   "sec", "Release", id.accent);
    }
    else if (kind == generic)
    {
        for (auto* p : pickMainParams (instrument, 4))
        {
            auto k = std::make_unique<ParamKnob> (session, p);
            addAndMakeVisible (*k);
            knobs.push_back (std::move (k));
        }
    }

    startTimerHz (15);
}

InstrumentCard::~InstrumentCard()
{
    stopTimer();
    knobs.clear();
    setLookAndFeel (nullptr);
}

int InstrumentCard::getIdealHeight() const
{
    int h = 20 + 30;

    if (kind != generic)
        h += 58;

    if (! knobs.empty())
        h += 70;

    return h;
}

void InstrumentCard::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop (30);

    if (popButton.isVisible())
    {
        popButton.setBounds (top.removeFromRight (28).withSizeKeepingCentre (28, 28));
        top.removeFromRight (6);
    }

    changeButton.setBounds (top.removeFromRight (64).withSizeKeepingCentre (64, 24));
    top.removeFromRight (6);
    titleArea = top;

    r.removeFromTop (6);
    displayArea = {};

    if (kind != generic)
    {
        displayArea = r.removeFromTop (52);
        r.removeFromTop (6);
    }

    if (! knobs.empty())
    {
        const int w = r.getWidth() / (int) knobs.size();

        for (auto& k : knobs)
            k->setBounds (r.removeFromLeft (w));
    }
}

void InstrumentCard::mouseDown (const juce::MouseEvent& e)
{
    if ((titleArea.contains (e.getPosition()) || displayArea.contains (e.getPosition())) && onOpen)
        onOpen();
}

//==============================================================================
void InstrumentCard::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);

    juce::ColourGradient bg (id.top, 0.0f, b.getY(), id.bottom, 0.0f, b.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (b, 12.0f);
    g.setColour (id.accent.withAlpha (0.4f));
    g.drawRoundedRectangle (b, 12.0f, 1.0f);

    paintTitle (g, titleArea.toFloat());

    if (! displayArea.isEmpty())
        paintDisplay (g, displayArea.toFloat());
}

void InstrumentCard::paintTitle (juce::Graphics& g, juce::Rectangle<float> a)
{
    switch (kind)
    {
        case eight08:
        {
            g.setFont (KngFonts::font (KngFonts::barlowHeavy, 26.0f));
            g.setColour (id.accent.withAlpha (0.18f));
            g.drawText ("808", a.translated (0.0f, 1.5f), juce::Justification::centredLeft, false);
            g.setColour (id.accent);
            g.drawText ("808", a, juce::Justification::centredLeft, false);

            g.setFont (KngFonts::font (KngFonts::barlowSemi, 11.0f, 0.2f));
            g.setColour (juce::Colour (0xff7d766e));
            g.drawText ("KNG BASS MACHINE", a.withTrimmedLeft (46.0f), juce::Justification::centredLeft, true);
            break;
        }

        case synth:
        {
            g.setFont (KngFonts::font (KngFonts::spaceGrotesk, 17.0f));
            g.setColour (juce::Colour (0xffe6f7ff));
            g.drawText ("KNG", a, juce::Justification::centredLeft, false);
            g.setColour (id.accent);
            g.drawText ("Synth", a.withTrimmedLeft (40.0f), juce::Justification::centredLeft, false);
            break;
        }

        case sampler:
        {
            g.setFont (KngFonts::font (KngFonts::sora, 16.0f));
            g.setColour (juce::Colour (0xfff1ecff));
            g.drawText ("KNG", a, juce::Justification::centredLeft, false);
            g.setColour (id.accent);
            g.drawText ("Sampler", a.withTrimmedLeft (40.0f), juce::Justification::centredLeft, false);

            const int root = (int) plugin->state.getProperty (KngSamplerPlugin::rootProp, 60);
            g.setFont (KngFonts::font (KngFonts::mono, 11.0f));
            g.setColour (juce::Colour (0xff6d6684));
            g.drawText (Theory::noteName (root), a.withTrimmedLeft (118.0f), juce::Justification::centredLeft, false);
            break;
        }

        case generic:
        default:
        {
            g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawText (plugin->getName(), a, juce::Justification::centredLeft, true);
            break;
        }
    }
}

void InstrumentCard::paintDisplay (juce::Graphics& g, juce::Rectangle<float> d)
{
    g.setColour (id.bottom.darker (0.6f));
    g.fillRoundedRectangle (d, 6.0f);
    g.setColour (id.accent.withAlpha (0.2f));
    g.drawRoundedRectangle (d.reduced (0.5f), 6.0f, 1.0f);

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (d.reduced (1.0f).toNearestInt());
    const auto a = d.reduced (6.0f, 5.0f);

    if (auto* p808 = dynamic_cast<Kng808Plugin*> (plugin.get()))
    {
        // Pitch drop (orange) over the volume fade (cream)
        const float lengthS = juce::jmax (0.01f, p808->getParam (Kng808Plugin::length)->getCurrentValue() * 0.001f);
        const float dropSt = p808->getParam (Kng808Plugin::drop)->getCurrentValue();
        const float tau = juce::jmax (0.0005f, p808->getParam (Kng808Plugin::dropTime)->getCurrentValue() * 0.001f / 3.0f);
        const float span = juce::jmax (0.3f, lengthS * 1.1f);
        juce::Path volume, pitch;

        for (int k = 0; k <= 80; ++k)
        {
            const float t = span * (float) k / 80.0f;
            const float x = a.getX() + a.getWidth() * (float) k / 80.0f;
            const float vy = a.getBottom() - std::exp (-6.9f * t / lengthS) * a.getHeight();
            const float py = a.getBottom() - (0.08f + juce::jlimit (0.0f, 1.0f, dropSt * std::exp (-t / tau) / 24.0f) * 0.86f) * a.getHeight();

            if (k == 0) { volume.startNewSubPath (x, vy); pitch.startNewSubPath (x, py); }
            else        { volume.lineTo (x, vy);          pitch.lineTo (x, py); }
        }

        g.setColour (juce::Colour (0xfff4ede4).withAlpha (0.6f));
        g.strokePath (volume, juce::PathStrokeType (1.5f));
        g.setColour (id.accent);
        g.strokePath (pitch, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else if (auto* syn = dynamic_cast<KngSynthPlugin*> (plugin.get()))
    {
        // The filter curve
        const double fc = juce::jmax (20.0f, syn->getParam (KngSynth::cutoff)->getCurrentValue());
        const double q = 1.0 / juce::jmax (0.04, 2.0 - 1.96 * (double) syn->getParam (KngSynth::reso)->getCurrentValue());
        const int type = juce::roundToInt (syn->getParam (KngSynth::filterType)->getCurrentValue());
        const juce::Colour pink (0xffff5fa8);
        juce::Path curve;

        for (int k = 0; k <= 100; ++k)
        {
            const double f = 20.0 * std::pow (1000.0, k / 100.0);
            const double w = f / fc;
            const double den = std::sqrt ((1.0 - w * w) * (1.0 - w * w) + (w / q) * (w / q));
            const double mag = type == 0 ? 1.0 / den : type == 1 ? (w * w) / den : (w / q) / den;
            const double db = juce::jlimit (-36.0, 18.0, 20.0 * std::log10 (juce::jmax (1.0e-6, mag)));
            const float x = a.getX() + a.getWidth() * (float) k / 100.0f;
            const float y = a.getY() + (float) ((18.0 - db) / 54.0) * a.getHeight();

            if (k == 0) curve.startNewSubPath (x, y);
            else        curve.lineTo (x, y);
        }

        auto fill = curve;
        fill.lineTo (a.getRight(), d.getBottom());
        fill.lineTo (a.getX(), d.getBottom());
        fill.closeSubPath();
        g.setColour (pink.withAlpha (0.2f));
        g.fillPath (fill);
        g.setColour (pink);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else if (auto* smp = dynamic_cast<KngSamplerPlugin*> (plugin.get()))
    {
        // The waveform as bars
        auto buffer = smp->getSampleForDisplay();
        const int bars = juce::jmax (1, (int) a.getWidth() / 3);

        if (buffer.get() != peaksSource || (int) peaks.size() != bars)
        {
            peaksSource = buffer.get();
            peaks.assign ((size_t) bars, 0.0f);

            if (buffer != nullptr && buffer->getNumSamples() > 0)
            {
                const int len = buffer->getNumSamples();
                const float* data = buffer->getReadPointer (0);
                float loudest = 0.0f;

                for (int x = 0; x < bars; ++x)
                {
                    const int s = (int) ((juce::int64) x * len / bars);
                    const int e = juce::jmax (s + 1, (int) ((juce::int64) (x + 1) * len / bars));
                    float peak = 0.0f;

                    for (int i = s; i < e && i < len; ++i)
                        peak = juce::jmax (peak, std::abs (data[i]));

                    peaks[(size_t) x] = peak;
                    loudest = juce::jmax (loudest, peak);
                }

                if (loudest > 0.0f)
                    for (auto& pk : peaks)
                        pk /= loudest;
            }
        }

        if (buffer == nullptr)
        {
            g.setFont (KngFonts::font (KngFonts::sora, 11.0f));
            g.setColour (juce::Colour (0xff6d6684));
            g.drawText ("No sample yet", a, juce::Justification::centred, false);
            return;
        }

        g.setColour (juce::Colour (0xff8a70ee));

        for (int x = 0; x < bars; ++x)
        {
            const float h = juce::jmax (2.0f, peaks[(size_t) x] * a.getHeight());
            g.fillRoundedRectangle (a.getX() + x * 3.0f, a.getCentreY() - h * 0.5f, 2.0f, h, 1.0f);
        }
    }
}
