#include "PatternListPanel.h"

PatternListPanel::PatternListPanel (EditSession& sessionToUse)
    : session (sessionToUse)
{
    addAndMakeVisible (newButton);
    newButton.setTooltip ("Start a new, empty pattern");
    newButton.onClick = [this]
    {
        session.newPattern();

        if (onPatternPicked)
            onPatternPicked();

        resized();
        repaint();
    };

    setTooltip ("Click a pattern to edit it in the Rack. The arrow loops it. Double-click to rename, right-click for color and more.");
    startTimerHz (5);
}

PatternListPanel::~PatternListPanel()
{
    stopTimer();
}

juce::Rectangle<int> PatternListPanel::rowBounds (int pattern)
{
    return { 6, headerHeight + (pattern - 1) * (rowHeight + 2), juce::jmax (0, getWidth() - 12), rowHeight };
}

int PatternListPanel::patternAt (int y)
{
    for (int p = 1; p <= session.getNumPatterns(); ++p)
    {
        const auto r = rowBounds (p);

        if (y >= r.getY() && y < r.getBottom())
            return p;
    }

    return 0;
}

void PatternListPanel::timerCallback()
{
    const int count = session.getNumPatterns();

    if (count != lastCount)
    {
        lastCount = count;
        resized();
    }

    // Playback never runs on into the pattern bank
    auto& transport = session.getTransport();

    if (transport.isPlaying() && ! transport.isRecording() && ! session.isLooping())
    {
        const double pos = transport.getPosition().inSeconds();
        const double bank = session.getBankStart();

        if (pos > bank - 5.0 && pos < bank)
            session.stop();
    }

    repaint();
}

//==============================================================================
void PatternListPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff24282c));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText ("PATTERNS", 12, 0, getWidth() - 24, headerHeight, juce::Justification::centredLeft);

    const int current = session.getCurrentPattern();

    for (int p = 1; p <= session.getNumPatterns(); ++p)
    {
        auto r = rowBounds (p);

        if (p == current)
        {
            g.setColour (juce::Colour (0xff2f3d4c));
            g.fillRoundedRectangle (r.toFloat(), 5.0f);
            g.setColour (juce::Colour (0xff3a8ee6));
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 5.0f, 1.0f);
        }

        auto inner = r.reduced (8, 0);

        // Loop arrow
        auto play = inner.removeFromRight (12).toFloat();
        juce::Path tri;
        tri.addTriangle (play.getX(), play.getCentreY() - 5.0f, play.getX(), play.getCentreY() + 5.0f,
                         play.getX() + 9.0f, play.getCentreY());
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.fillPath (tri);
        inner.removeFromRight (6);

        const int bars = session.getPatternBars (p);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.drawText (juce::String (bars) + (bars == 1 ? " bar" : " bars"), inner.removeFromRight (40),
                    juce::Justification::centredRight);

        auto swatch = inner.removeFromLeft (12);
        g.setColour (session.getPatternColour (p));
        g.fillRoundedRectangle (swatch.withSizeKeepingCentre (12, 12).toFloat(), 3.0f);
        inner.removeFromLeft (8);

        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawText (session.getPatternName (p), inner, juce::Justification::centredLeft, true);
    }
}

void PatternListPanel::resized()
{
    const int y = headerHeight + session.getNumPatterns() * (rowHeight + 2) + 4;
    newButton.setBounds (6, y, juce::jmax (0, getWidth() - 12), 30);
}

//==============================================================================
void PatternListPanel::mouseDown (const juce::MouseEvent& e)
{
    const int p = patternAt (e.y);

    if (p <= 0)
        return;

    session.setCurrentPattern (p);

    if (e.mods.isPopupMenu())
    {
        showMenu (p);
        repaint();
        return;
    }

    // The arrow loops just this pattern
    if (e.x >= rowBounds (p).getRight() - 26)
        session.loopPattern (p);

    if (onPatternPicked)
        onPatternPicked();

    repaint();
}

void PatternListPanel::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int p = patternAt (e.y);

    if (p > 0 && e.x < rowBounds (p).getRight() - 26)
        rename (p);
}

void PatternListPanel::rename (int pattern)
{
    auto* s = &session;
    auto* w = new juce::AlertWindow ("Rename Pattern", "Name:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", session.getPatternName (pattern));
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create ([s, pattern, w] (int result)
    {
        if (result == 1)
            s->setPatternName (pattern, w->getTextEditorContents ("name"));
    }), true);
}

void PatternListPanel::showMenu (int pattern)
{
    auto* s = &session;
    juce::Component::SafePointer<PatternListPanel> safe (this);
    juce::PopupMenu menu;

    menu.addItem ("Rename...", [safe, pattern] { if (safe != nullptr) safe->rename (pattern); });

    struct Swatch { const char* name; juce::uint32 argb; };
    const Swatch swatches[] = {
        { "Blue",   0xff3a6ea5 }, { "Red",    0xffc0392b }, { "Orange", 0xffd35400 },
        { "Yellow", 0xffc9a227 }, { "Green",  0xff2e8b57 }, { "Teal",   0xff16a085 },
        { "Purple", 0xff8e44ad }, { "Pink",   0xffc2185b }, { "Grey",   0xff607d8b }
    };

    juce::PopupMenu colours;

    for (auto& sw : swatches)
    {
        const juce::Colour c (sw.argb);
        colours.addItem (sw.name, [s, pattern, c] { s->setPatternColour (pattern, c); });
    }

    menu.addSubMenu ("Color", colours);
    menu.addItem ("Duplicate", [s, safe, pattern]
    {
        if (s->duplicatePattern (pattern) > 0 && safe != nullptr && safe->onPatternPicked)
            safe->onPatternPicked();
    });
    menu.addItem ("Loop It", [s, pattern] { s->loopPattern (pattern); });
    menu.showMenuAsync ({});
}
