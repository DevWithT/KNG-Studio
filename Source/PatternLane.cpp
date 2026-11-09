#include "PatternLane.h"

PatternLane::PatternLane (EditSession& sessionToUse)
    : session (sessionToUse)
{
    setTooltip ("Click or drag across bars to paint the current pattern. Drag a block to move it, "
                "Ctrl+drag to copy it, right-drag to erase.");
    startTimerHz (10);
}

PatternLane::~PatternLane()
{
    stopTimer();
}

juce::Colour PatternLane::colourFor (int pattern)
{
    return juce::Colour::fromHSV (std::fmod (0.58f + (pattern - 1) * 0.13f, 1.0f), 0.55f, 0.85f, 1.0f);
}

double PatternLane::barStartAt (int x)
{
    const double bar = session.getBarSeconds();
    const double t = juce::jmax (0.0, session.xToTime (x, getWidth()));
    return std::floor (t / bar) * bar;
}

void PatternLane::paintAt (int x)
{
    const double start = barStartAt (x);

    if (std::abs (start - lastBar) < 0.001)
        return;

    lastBar = start;

    const int pattern = session.getCurrentPattern();
    const double length = session.getBarSeconds() * session.getPatternBars (pattern);

    for (auto& b : session.getPatternBlocks())
        if (b.start < start + length - 0.001 && b.end > start + 0.001)
            return;

    session.placePattern (pattern, start);
}

void PatternLane::eraseAt (int x)
{
    session.removePatternAt (juce::jmax (0.0, session.xToTime (x, getWidth())));
}

//==============================================================================
void PatternLane::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2124));

    const double bar = session.getBarSeconds();
    const double viewEnd = session.getViewStart() + session.getViewLength();

    g.setColour (juce::Colours::white.withAlpha (0.06f));

    for (double t = std::floor (session.getViewStart() / bar) * bar; t <= viewEnd; t += bar)
        g.drawVerticalLine (session.timeToX (t, getWidth()), 0.0f, (float) getHeight());

    auto blocks = session.getPatternBlocks();
    g.setFont (juce::Font (juce::FontOptions (12.0f)));

    for (auto& b : blocks)
    {
        const int x1 = session.timeToX (b.start, getWidth());
        const int x2 = session.timeToX (b.end, getWidth());

        if (x2 < 0 || x1 > getWidth())
            continue;

        auto r = juce::Rectangle<int> (x1, 2, juce::jmax (4, x2 - x1), getHeight() - 4).toFloat().reduced (1.0f, 0.0f);
        const bool current = b.pattern == session.getCurrentPattern();
        const bool beingMoved = moving && b.pattern == movePattern && std::abs (b.start - moveFrom) < 0.001;

        g.setColour (colourFor (b.pattern).withAlpha (beingMoved ? 0.3f : current ? 0.9f : 0.55f));
        g.fillRoundedRectangle (r, 4.0f);

        if (current)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawRoundedRectangle (r, 4.0f, 1.5f);
        }

        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.drawText ("Pattern " + juce::String (b.pattern), r.toNearestInt().reduced (6, 0),
                    juce::Justification::centredLeft, true);
    }

    // Where a dragged block will land
    if (moving && std::abs (moveTo - moveFrom) > 0.001)
    {
        const int x1 = session.timeToX (moveTo, getWidth());
        const int x2 = session.timeToX (moveTo + moveLength, getWidth());
        auto r = juce::Rectangle<int> (x1, 2, juce::jmax (4, x2 - x1), getHeight() - 4).toFloat();

        g.setColour (colourFor (movePattern).withAlpha (0.6f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colours::white);
        g.drawRoundedRectangle (r, 4.0f, 1.5f);
    }

    if (blocks.empty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.drawText ("Patterns: pick one in the Rack, then click or drag here to paint it. Drag a block to move, Ctrl+drag to copy.",
                    getLocalBounds().reduced (8, 0), juce::Justification::centredLeft);
    }
}

//==============================================================================
void PatternLane::mouseDown (const juce::MouseEvent& e)
{
    lastBar = -1.0;
    moving = painting = erasing = false;

    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
    {
        erasing = true;
        eraseAt (e.x);
        return;
    }

    const double t = session.xToTime (e.x, getWidth());

    // Grab a block: select its pattern and start dragging it
    for (auto& b : session.getPatternBlocks())
    {
        if (t >= b.start && t < b.end)
        {
            session.setCurrentPattern (b.pattern);
            moving = true;
            movePattern = b.pattern;
            moveFrom = moveTo = b.start;
            moveLength = b.end - b.start;
            moveGrab = t - b.start;
            return;
        }
    }

    painting = true;
    paintAt (e.x);
}

void PatternLane::mouseDrag (const juce::MouseEvent& e)
{
    if (erasing)
    {
        eraseAt (e.x);
    }
    else if (painting)
    {
        paintAt (e.x);
    }
    else if (moving)
    {
        const double bar = session.getBarSeconds();
        const double t = session.xToTime (e.x, getWidth()) - moveGrab;
        moveTo = juce::jmax (0.0, std::round (t / bar) * bar);
        repaint();
    }
}

void PatternLane::mouseUp (const juce::MouseEvent& e)
{
    if (moving && std::abs (moveTo - moveFrom) > 0.001)
    {
        if (e.mods.isCtrlDown())
            session.placePattern (movePattern, moveTo);   // Ctrl: leave the original, drop a copy
        else
            session.movePatternBlock (movePattern, moveFrom, moveTo);
    }

    painting = erasing = moving = false;
    lastBar = -1.0;
    repaint();
}

void PatternLane::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const float d = std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? wheel.deltaX : wheel.deltaY;
    session.scrollBy (-d * session.getViewLength() * 0.15);
}
