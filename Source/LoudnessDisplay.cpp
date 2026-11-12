#include "LoudnessDisplay.h"
#include "KngLoudnessPlugin.h"

LoudnessDisplay::LoudnessDisplay (EditSession& sessionToUse)
    : session (sessionToUse)
{
    setTooltip ("Loudness the way Spotify and YouTube measure it. Aim for I around -14 LUFS. Click to reset, then play the whole song.");
    blocks.reserve (36000);
    startTimerHz (10);
}

LoudnessDisplay::~LoudnessDisplay()
{
    stopTimer();
}

double LoudnessDisplay::toLufs (double meanSquare)
{
    return -0.691 + 10.0 * std::log10 (juce::jmax (meanSquare, 1.0e-12));
}

double LoudnessDisplay::average (int lastN) const
{
    const int n = juce::jmin (lastN, (int) blocks.size());

    if (n == 0)
        return 0.0;

    double sum = 0.0;

    for (int i = (int) blocks.size() - n; i < (int) blocks.size(); ++i)
        sum += blocks[(size_t) i];

    return sum / n;
}

double LoudnessDisplay::integrated() const
{
    // 400 ms windows stepping 100 ms, then the absolute (-70) and relative (-10) gates
    std::vector<double> windows;

    for (size_t i = 3; i < blocks.size(); ++i)
        windows.push_back ((blocks[i] + blocks[i - 1] + blocks[i - 2] + blocks[i - 3]) * 0.25);

    double sum = 0.0;
    int n = 0;

    for (double z : windows)
    {
        if (toLufs (z) > -70.0)
        {
            sum += z;
            ++n;
        }
    }

    if (n == 0)
        return -100.0;

    const double relativeGate = toLufs (sum / n) - 10.0;
    sum = 0.0;
    n = 0;

    for (double z : windows)
    {
        const double l = toLufs (z);

        if (l > -70.0 && l > relativeGate)
        {
            sum += z;
            ++n;
        }
    }

    return n > 0 ? toLufs (sum / n) : -100.0;
}

void LoudnessDisplay::timerCallback()
{
    auto* meter = session.getLoudnessMeter();

    if (meter == nullptr)
        return;

    float incoming[256];
    const int n = meter->pullBlocks (incoming, 256);

    for (int i = 0; i < n && blocks.size() < 36000; ++i)
        blocks.push_back (incoming[i]);

    momentary = blocks.size() >= 4  ? toLufs (average (4))  : -100.0;
    shortTerm = blocks.size() >= 30 ? toLufs (average (30)) : -100.0;
    whole = integrated();

    const float p = juce::Decibels::gainToDecibels (meter->takePeak(), -100.0f);
    peakDb = p > peakDb ? p : peakDb - 0.3f;

    repaint();
}

void LoudnessDisplay::mouseDown (const juce::MouseEvent&)
{
    blocks.clear();
    momentary = shortTerm = whole = -100.0;
    peakDb = -100.0f;
    repaint();
}

void LoudnessDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff1b1d20));
    g.fillRoundedRectangle (r, 4.0f);

    auto text = [] (double v) { return v <= -99.0 ? juce::String ("--") : juce::String (v, 1); };
    auto area = getLocalBounds().reduced (8, 0);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));

    auto cell = [&] (const juce::String& label, const juce::String& value, juce::Colour colour, int width)
    {
        auto c = area.removeFromLeft (width);
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.drawText (label, c.removeFromLeft (14), juce::Justification::centredLeft);
        g.setColour (colour);
        g.drawText (value, c, juce::Justification::centredLeft);
    };

    // Integrated: green near the -14 streaming target, orange louder, blue quieter
    const auto wholeColour = whole <= -99.0 ? juce::Colours::white.withAlpha (0.5f)
                           : std::abs (whole + 14.0) <= 1.0 ? juce::Colour (0xff5ad45a)
                           : whole > -14.0 ? juce::Colour (0xffe6893a)
                                           : juce::Colour (0xff3a8ee6);

    cell ("M", text (momentary), juce::Colours::white.withAlpha (0.85f), 54);
    cell ("S", text (shortTerm), juce::Colours::white.withAlpha (0.85f), 54);
    cell ("I", text (whole) + " LUFS", wholeColour, 96);
    cell ("P", text (peakDb), peakDb > -1.0f ? juce::Colour (0xffe6503a) : juce::Colours::white.withAlpha (0.85f), 54);
}
