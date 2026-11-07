#include "RackView.h"

namespace te = tracktion;

RackView::RackView (EditSession& sessionToUse)
    : session (sessionToUse)
{
    for (auto* c : std::initializer_list<juce::Component*> { &patternLabel, &patternBox, &barsLabel, &barsBox,
                                                             &placeButton, &loopButton, &addDrumButton,
                                                             &addSamplerButton, &addInstrumentButton, &hintLabel,
                                                             &viewport })
        addAndMakeVisible (c);

    patternBox.onChange = [this]
    {
        const int id = patternBox.getSelectedId();

        if (id == 1000)
            session.newPattern();
        else if (id > 0)
            session.setCurrentPattern (id);

        builtPatternCount = 0;   // forces the list to refresh
        syncToolbar();
    };

    barsBox.addItem ("1", 1);
    barsBox.addItem ("2", 2);
    barsBox.addItem ("4", 4);
    barsBox.onChange = [this] { session.setPatternBars (session.getCurrentPattern(), barsBox.getSelectedId()); };

    placeButton.onClick = [this]
    {
        session.placePattern (session.getCurrentPattern(), session.getTransport().getPosition().inSeconds());
    };

    loopButton.onClick = [this] { session.loopPattern (session.getCurrentPattern()); };

    addDrumButton.onClick       = [this] { session.addDrumTrack(); };
    addInstrumentButton.onClick = [this] { session.addMidiTrack(); };
    addSamplerButton.onClick    = [this]
    {
        session.addMidiTrack();
        session.setBuiltInSampler (session.getNumTracks() - 1);
    };

    hintLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    hintLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.5f));
    hintLabel.setText ("Click steps for drums, click a melodic row for notes. Copies on the timeline stay linked.",
                       juce::dontSendNotification);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);

    rebuild();
    syncToolbar();
    startTimerHz (4);
}

RackView::~RackView()
{
    stopTimer();
    viewport.setViewedComponent (nullptr, false);
    rows.clear();
}

juce::Array<te::Track*> RackView::getRackTracks()
{
    juce::Array<te::Track*> result;

    for (int i = 0; i < session.getNumTracks(); ++i)
        if (session.isMidiTrack (i))
            result.add (session.getTrack (i));

    return result;
}

void RackView::timerCallback()
{
    if (getRackTracks() != builtTracks)
    {
        rebuild();
        return;
    }

    // Rows grow when a kit loads or a drum row opens
    juce::String sig;

    for (auto* row : rows)
        sig << row->getIdealHeight() << ",";

    if (sig != lastLayoutSignature)
        layoutRows();

    syncToolbar();
}

void RackView::rebuild()
{
    rows.clear();
    builtTracks = getRackTracks();

    for (auto* t : builtTracks)
    {
        auto* row = rows.add (new RackRow (session, *t));
        row->onLayoutChanged = [this] { layoutRows(); };
        content.addAndMakeVisible (row);
    }

    layoutRows();
}

void RackView::layoutRows()
{
    const int width = juce::jmax (0, viewport.getWidth() - viewport.getScrollBarThickness());
    int y = 0;
    juce::String sig;

    for (auto* row : rows)
    {
        const int h = row->getIdealHeight();
        row->setBounds (0, y, width, h);
        y += h + 4;
        sig << h << ",";
    }

    lastLayoutSignature = sig;
    content.setSize (width, juce::jmax (y, 1));
}

void RackView::syncToolbar()
{
    const int count = session.getNumPatterns();
    const int current = session.getCurrentPattern();

    if (count != builtPatternCount)
    {
        builtPatternCount = count;
        patternBox.clear (juce::dontSendNotification);

        for (int i = 1; i <= count; ++i)
            patternBox.addItem ("Pattern " + juce::String (i), i);

        patternBox.addSeparator();
        patternBox.addItem ("New pattern", 1000);
    }

    if (! patternBox.isPopupActive() && patternBox.getSelectedId() != current)
        patternBox.setSelectedId (current, juce::dontSendNotification);

    if (! barsBox.isPopupActive())
        barsBox.setSelectedId (session.getPatternBars (current), juce::dontSendNotification);
}

void RackView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d20));

    g.setColour (juce::Colour (0xff26292d));
    g.fillRect (getLocalBounds().removeFromTop (toolbarHeight));

    if (rows.isEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        g.drawText ("No instruments yet. Add Drums, a Sampler, or an Instrument up top.",
                    getLocalBounds().withTrimmedTop (toolbarHeight), juce::Justification::centred);
    }
}

void RackView::resized()
{
    auto tb = getLocalBounds().removeFromTop (toolbarHeight).reduced (6);

    patternLabel.setBounds (tb.removeFromLeft (56));
    patternBox.setBounds (tb.removeFromLeft (130));
    tb.removeFromLeft (10);
    barsLabel.setBounds (tb.removeFromLeft (36));
    barsBox.setBounds (tb.removeFromLeft (60));
    tb.removeFromLeft (10);
    placeButton.setBounds (tb.removeFromLeft (130));
    tb.removeFromLeft (4);
    loopButton.setBounds (tb.removeFromLeft (110));
    tb.removeFromLeft (16);
    addDrumButton.setBounds (tb.removeFromLeft (80));
    tb.removeFromLeft (4);
    addSamplerButton.setBounds (tb.removeFromLeft (90));
    tb.removeFromLeft (4);
    addInstrumentButton.setBounds (tb.removeFromLeft (110));
    tb.removeFromLeft (12);
    hintLabel.setBounds (tb);

    viewport.setBounds (getLocalBounds().withTrimmedTop (toolbarHeight).reduced (6));
    layoutRows();
}
