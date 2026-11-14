#include "KngKeysView.h"
#include "KngKeysPlugin.h"

namespace te = tracktion;

static bool isBlack (int note)
{
    const int n = note % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

KngKeysView::KngKeysView (KngKeysPlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xffe8b04a, 0xff4a3826, 0xff1a130d, 0xfffff4e0, 0xffc7a985, 0xfff5d08a, 0xff1f170f, 0xff4a3826,
                                 KngFonts::sora, false),
                   { juce::Colour (0xff0d0906), juce::Colour (0xff2e2013), juce::Colour (0xff150f09), juce::Colour (0xff45311d) }),
      fx (owner)
{
    titleWidth = 160;
    sideWidth = 40;
    maxKnobWidth = 96;

    juce::StringArray names;

    for (auto& preset : fx.getPresets())
        names.add (preset.name);

    attachPresets (names, [this] (int i) { fx.applyPreset (i); });

    addHeaderChoice (*fx.getParam (KngKeysPlugin::sound), 0.0f, KngKeysPlugin::soundNames().joinIntoString ("|"), 300);

    addKnob (fx, KngKeysPlugin::tone, "Tone");
    addKnob (fx, KngKeysPlugin::character, "Character");
    addKnob (fx, KngKeysPlugin::decay, "Decay");
    addKnob (fx, KngKeysPlugin::motion, "Motion");

    auto& speedKnob = addKnob (fx, KngKeysPlugin::speed, "Speed");
    speedKnob.formatter = [] (float v) { return juce::String (v, 1) + " Hz"; };

    addKnob (fx, KngKeysPlugin::width, "Width");
    addKnob (fx, KngKeysPlugin::velSens, "Velocity");
    addKnob (fx, KngKeysPlugin::level, "Volume");
}

void KngKeysView::tick()
{
    float in = 0.0f, out = 0.0f, gr = 0.0f;
    fx.takeMeters (in, out, gr);
    outDb = fallingDb (out, outDb);
}

void KngKeysView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "Keys", KngFonts::sora, 30.0f);
}

void KngKeysView::paintSide (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawInOut (g, area, -100.0f, outDb);
}

void KngKeysView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawScreen (g, area);

    auto inner = area.reduced (14.0f, 10.0f);
    auto top = inner.removeFromTop (34.0f);
    inner.removeFromTop (6.0f);

    // The sound's name, big, with a word on what Character and Motion do for it
    const int s = juce::jlimit (0, 2, juce::roundToInt (fx.getParam (KngKeysPlugin::sound)->getCurrentValue()));
    const char* notes[] = { "Character: hammer    Motion: off", "Character: bark    Motion: tremolo", "Character: drawbars    Motion: rotary speaker" };

    g.setFont (KngFonts::font (KngFonts::sora, 24.0f));
    g.setColour (style.accent);
    g.drawText (KngKeysPlugin::soundNames()[s].toUpperCase(), top, juce::Justification::centredLeft, false);
    g.setFont (KngFonts::font (KngFonts::mono, 11.0f));
    g.setColour (style.label);
    g.drawText (notes[s], top, juce::Justification::centredRight, false);

    // Wooden cheek blocks on either side of the keys
    const float cheek = juce::jmin (26.0f, inner.getWidth() * 0.04f);
    auto keys = inner.reduced (cheek, 0.0f);

    for (auto block : { inner.withWidth (cheek - 2.0f), inner.withTrimmedLeft (inner.getWidth() - cheek + 2.0f) })
    {
        juce::ColourGradient wood (juce::Colour (0xff6b4423), block.getX(), block.getY(), juce::Colour (0xff3a2412), block.getRight(), block.getBottom(), false);
        g.setGradientFill (wood);
        g.fillRoundedRectangle (block, 4.0f);
    }

    // Four octaves, C2 to C6
    constexpr int lowNote = 36, highNote = 84;
    int whiteCount = 0;

    for (int n = lowNote; n <= highNote; ++n)
        if (! isBlack (n))
            ++whiteCount;

    const float whiteW = keys.getWidth() / (float) whiteCount;
    const float blackW = whiteW * 0.62f, blackH = keys.getHeight() * 0.6f;
    const auto lit = style.accent;

    int w = 0;

    for (int n = lowNote; n <= highNote; ++n)
    {
        if (isBlack (n))
            continue;

        auto r = juce::Rectangle<float> (keys.getX() + w * whiteW, keys.getY(), whiteW, keys.getHeight()).reduced (0.75f, 0.0f);
        g.setColour (fx.isNoteLit (n) ? lit : juce::Colour (0xfff4efe6));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);

        if (n % 12 == 0)
        {
            g.setFont (KngFonts::font (KngFonts::mono, 9.0f));
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.drawText ("C" + juce::String (n / 12 - 1), r.removeFromBottom (14.0f), juce::Justification::centred, false);
        }

        ++w;
    }

    w = 0;

    for (int n = lowNote; n <= highNote; ++n)
    {
        if (! isBlack (n))
        {
            ++w;
            continue;
        }

        const float x = keys.getX() + w * whiteW - blackW * 0.5f;
        auto r = juce::Rectangle<float> (x, keys.getY(), blackW, blackH);
        g.setColour (fx.isNoteLit (n) ? lit.darker (0.2f) : juce::Colour (0xff1a1410));
        g.fillRoundedRectangle (r, 2.5f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (r.reduced (2.0f).withHeight (r.getHeight() * 0.3f), 2.0f);
    }

    // A warm glow over the keys while it plays
    const float level = juce::jlimit (0.0f, 1.0f, (outDb + 40.0f) / 40.0f);

    if (level > 0.01f)
    {
        g.setColour (style.accent.withAlpha (0.08f * level));
        g.fillRect (keys);
    }
}
