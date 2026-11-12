#include "KngDelayView.h"
#include "KngDelayPlugin.h"

namespace te = tracktion;

static const juce::Colour rightColour (0xffffb38a);

static KngParamDef delayDef (int p, const char* unit)
{
    const auto& in = KngDelay::info (p);
    return { "", "", (float) in.min, (float) in.max, (float) in.def, (float) in.mid, (float) in.step, unit, nullptr };
}

static juce::String divisionText (float v)
{
    const auto& names = KngDelay::divisionNames();
    return names.isEmpty() ? juce::String() : names[juce::jlimit (0, names.size() - 1, juce::roundToInt (v))];
}

//==============================================================================
KngDelayView::KngDelayView (KngDelayPlugin& owner)
    : KngFaceView (owner,
                   kngMakeStyle (0xff22c3b5, 0xff1e3a3a, 0xff0a1616, 0xffe6fffb, 0xff7fb8b1, 0xff7ff0e4, 0xff0b1b1c, 0xff1d3a3a,
                                 KngFonts::spaceGrotesk, false),
                   { juce::Colour (0xff050c0d), juce::Colour (0xff0f2224), juce::Colour (0xff071213), juce::Colour (0xff1a3536) }),
      delay (owner)
{
    titleWidth = 170;
    sideWidth = 22;
    maxKnobWidth = 84;

    juce::StringArray names;

    for (auto& preset : KngDelay::presets())
        names.add (preset.name);

    attachPresets (names, [this] (int i) { delay.applyPreset (i); });

    using namespace KngDelay;

    addHeaderChoice (*delay.getParam (mode), (float) info (mode).min, modeNames().joinIntoString ("|"), 270);
    addHeaderSwitch (*delay.getParam (sync), "Sync", "Lock the echoes to the song's tempo", 100);

    // Left and right: milliseconds when free, note lengths when synced
    timeLKnob = &addKnob (*delay.getParam (timeL), delayDef (timeL, "ms"), "Left");
    timeRKnob = &addKnob (*delay.getParam (timeR), delayDef (timeR, "ms"), "Right");
    divLKnob  = &addKnob (*delay.getParam (divL),  delayDef (divL, ""),    "Left");
    divRKnob  = &addKnob (*delay.getParam (divR),  delayDef (divR, ""),    "Right");
    divLKnob->formatter = divisionText;
    divRKnob->formatter = divisionText;
    timeRKnob->setArcColour (rightColour);
    divRKnob->setArcColour (rightColour);

    addKnob (*delay.getParam (feedback), delayDef (feedback, "%"), "Feedback");
    addKnob (*delay.getParam (lowCut),   delayDef (lowCut, "Hz"),  "Low Cut");
    addKnob (*delay.getParam (highCut),  delayDef (highCut, "Hz"), "High Cut");
    addKnob (*delay.getParam (drive),    delayDef (drive, "%"),    "Tape");
    addKnob (*delay.getParam (wobble),   delayDef (wobble, "%"),   "Wobble");
    addKnob (*delay.getParam (width),    delayDef (width, "%"),    "Width");
    addKnob (*delay.getParam (duck),     delayDef (duck, "%"),     "Duck");
    addKnob (*delay.getParam (mix),      delayDef (mix, "%"),      "Mix");

    tick();
}

float KngDelayView::value (int p) const
{
    return delay.getParam (p)->getCurrentValue();
}

void KngDelayView::tick()
{
    wetDb = fallingDb (delay.takeWetPeak(), wetDb);
    clock += 1.0 / 30.0;

    const bool synced = value (KngDelay::sync) > 0.5f;
    const bool isMono = juce::roundToInt (value (KngDelay::mode)) == KngDelay::mono;

    timeLKnob->setVisible (! synced);
    timeRKnob->setVisible (! synced && ! isMono);
    divLKnob->setVisible (synced);
    divRKnob->setVisible (synced && ! isMono);
}

//==============================================================================
void KngDelayView::paintTitle (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawTitle (g, area, "Delay", KngFonts::spaceGrotesk, 28.0f);
}

void KngDelayView::paintSide (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawMeter (g, area, wetDb, "WET");
}

void KngDelayView::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawScreen (g, area);

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (area.reduced (1.0f).toNearestInt());

    const int m = juce::roundToInt (value (KngDelay::mode));
    const double dL = delay.getDelaySeconds (0);
    const double dR = m == KngDelay::mono ? dL : delay.getDelaySeconds (1);
    const float fb = value (KngDelay::feedback);
    const double window = juce::jlimit (1.0, 6.0, juce::jmax (dL, dR) * 7.0);
    const double beat = 60.0 / juce::jmax (30.0, delay.getBpm());

    const auto plot = area.reduced (14.0f).withTrimmedTop (22.0f);
    auto toX = [&plot, window] (double t) { return plot.getX() + (float) (t / window) * plot.getWidth(); };
    const float midY = plot.getCentreY();
    const float half = plot.getHeight() * 0.5f - 6.0f;

    // The song's beats, with every 4th brighter
    int beatIndex = 0;

    for (double t = 0.0; t <= window; t += beat, ++beatIndex)
    {
        g.setColour (juce::Colours::white.withAlpha (beatIndex % 4 == 0 ? 0.12f : 0.045f));
        g.drawVerticalLine ((int) toX (t), plot.getY(), plot.getBottom());
    }

    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawHorizontalLine ((int) midY, plot.getX(), plot.getRight());

    // A soft line sweeps across like a tape head
    const float sweepX = toX (std::fmod (clock, window));
    juce::ColourGradient sweep (style.accent.withAlpha (0.0f), sweepX - 60.0f, 0.0f, style.accent.withAlpha (0.18f), sweepX, 0.0f, false);
    g.setGradientFill (sweep);
    g.fillRect (sweepX - 60.0f, plot.getY(), 60.0f, plot.getHeight());

    // The dry hit
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.fillRoundedRectangle (toX (0.0) - 2.0f, midY - half, 4.0f, half * 2.0f, 2.0f);

    // Each echo, shrinking as it repeats (left on top, right underneath)
    auto drawTap = [&] (double t, float amp, bool isLeft)
    {
        if (t > window || amp < 0.01f)
            return;

        const float h = half * amp;
        const auto colour = isLeft ? style.accent : rightColour;
        const auto bar = juce::Rectangle<float> (toX (t) - 3.0f, isLeft ? midY - h : midY, 6.0f, h);

        g.setColour (colour.withAlpha (0.25f * amp));
        g.fillRoundedRectangle (bar.expanded (3.0f, 0.0f), 3.0f);
        g.setColour (colour.withAlpha (0.4f + 0.6f * amp));
        g.fillRoundedRectangle (bar, 3.0f);
    };

    for (int k = 1; k <= 24; ++k)
    {
        const float amp = std::pow (fb, (float) (k - 1));

        if (m == KngDelay::pingPong)
        {
            const int lefts = (k + 1) / 2, rights = k / 2;
            drawTap (lefts * dL + rights * dR, amp, k % 2 == 1);
        }
        else
        {
            drawTap (k * dL, amp, true);

            if (m != KngDelay::mono)
                drawTap (k * dR, amp, false);
        }
    }

    // Readout
    auto describe = [this] (int side, double seconds)
    {
        const auto ms = juce::String (juce::roundToInt (seconds * 1000.0)) + " ms";

        if (value (KngDelay::sync) < 0.5f)
            return ms;

        return divisionText (value (side == 0 ? KngDelay::divL : KngDelay::divR)) + " = " + ms;
    };

    juce::String text = "L  " + describe (0, dL);

    if (m != KngDelay::mono)
        text << "      R  " << describe (1, dR);

    text << "      " << juce::String (delay.getBpm(), 1) << " BPM";

    g.setFont (KngFonts::font (KngFonts::mono, 12.0f));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawText (text, area.reduced (14.0f, 8.0f).removeFromTop (16.0f), juce::Justification::centredLeft, false);
}
