#include "RackView.h"
#include "PluginBrowser.h"
#include "KitBrowser.h"

namespace te = tracktion;

static bool isAudioFile (const juce::File& f)
{
    return f.hasFileExtension ("wav;aif;aiff;flac;mp3;ogg");
}

//==============================================================================
RackView::RackView (EditSession& sessionToUse)
    : session (sessionToUse)
{
    for (auto* c : std::initializer_list<juce::Component*> { &patternBox, &barsBox, &swingLabel, &swingSlider, &viewport })
        addAndMakeVisible (c);

    // The pattern menu also holds Place at Playhead and Loop Pattern
    patternBox.onChange = [this]
    {
        const int id = patternBox.getSelectedId();
        const int current = session.getCurrentPattern();

        if (id == 1000)
            session.newPattern();
        else if (id == 1001)
            session.placePattern (current, session.getTransport().getPosition().inSeconds());
        else if (id == 1002)
            session.loopPattern (current);
        else if (id > 0)
            session.setCurrentPattern (id);

        builtPatternCount = 0;   // forces the list to refresh
        syncToolbar();
    };

    barsBox.addItem ("1 bar", 1);
    barsBox.addItem ("2 bars", 2);
    barsBox.addItem ("4 bars", 4);
    barsBox.onChange = [this] { session.setPatternBars (session.getCurrentPattern(), barsBox.getSelectedId()); };

    swingSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    swingSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
    swingSlider.setRange (0.0, 100.0, 1.0);
    swingSlider.setTextValueSuffix ("%");
    swingSlider.setDoubleClickReturnValue (true, 0.0);
    swingSlider.onDragStart   = [this] { session.beginEdit(); };
    swingSlider.onValueChange = [this] { session.setPatternSwing (session.getCurrentPattern(), (float) swingSlider.getValue()); };
    swingSlider.onDragEnd     = [this] { session.save(); };

    content.addAndMakeVisible (addRowButton);
    addRowButton.onClick = [this] { showAddMenu(); };

    // Tooltips
    patternBox.setTooltip ("Which pattern the Rack is editing. Also: New pattern, Place at Playhead, Loop Pattern.");
    barsBox.setTooltip ("How many bars long this pattern is");
    swingSlider.setTooltip ("Push every other 16th late for groove (drum and sound rows only)");
    addRowButton.setTooltip ("Add a sample or an instrument as a new row. You can also drag samples in from the Browser.");

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

void RackView::addSamples (const juce::Array<juce::File>& files)
{
    for (auto& f : files)
        if (f.existsAsFile() && isAudioFile (f))
            session.addSampleChannel (f);
}

void RackView::showAddMenu()
{
    juce::Component::SafePointer<RackView> safe (this);
    juce::PopupMenu menu;

    menu.addItem ("Sample from a file...", [safe]
    {
        if (safe == nullptr)
            return;

        safe->chooser = std::make_unique<juce::FileChooser> ("Add Samples", EditSession::getKitsFolder(),
                                                             "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                        | juce::FileBrowserComponent::canSelectMultipleItems,
                                    [safe] (const juce::FileChooser& fc)
                                    {
                                        if (safe != nullptr)
                                            safe->addSamples (fc.getResults());
                                    });
    });

    menu.addSeparator();
    menu.addSectionHeader ("Instruments");

    juce::PopupMenu yours;

    for (auto& e : PluginBrowser::buildEntries (session.getEdit().engine))
    {
        if (e.header || ! e.instrument)
            continue;

        const auto id = e.id;
        const auto name = e.name;
        auto add = [safe, id, name] { if (safe != nullptr) safe->session.addInstrumentTrack (id, name); };

        if (id.startsWith ("kng"))
            menu.addItem (name, add);
        else
            yours.addItem (name, add);
    }

    if (yours.getNumItems() > 0)
        menu.addSubMenu ("Your instruments", yours);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addRowButton));
}

//==============================================================================
void RackView::timerCallback()
{
    if (getRackTracks() != builtTracks)
    {
        rebuild();
        return;
    }

    // Rows grow when a drum row opens
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
    repaint();
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

    // The add row always sits at the bottom of the list
    addRowButton.setBounds (0, y, width, addRowHeight);
    y += addRowHeight;

    lastLayoutSignature = sig;
    content.setSize (width, juce::jmax (y, 1));
}

void RackView::syncToolbar()
{
    const int count = session.getNumPatterns();
    const int current = session.getCurrentPattern();

    juce::String names;

    for (int i = 1; i <= count; ++i)
        names << session.getPatternName (i) << ";";

    if (count != builtPatternCount || names != builtPatternNames)
    {
        builtPatternCount = count;
        builtPatternNames = names;
        patternBox.clear (juce::dontSendNotification);

        for (int i = 1; i <= count; ++i)
            patternBox.addItem (session.getPatternName (i), i);

        patternBox.addSeparator();
        patternBox.addItem ("New pattern", 1000);
        patternBox.addItem ("Place at Playhead", 1001);
        patternBox.addItem ("Loop Pattern", 1002);
    }

    if (! patternBox.isPopupActive() && patternBox.getSelectedId() != current)
        patternBox.setSelectedId (current, juce::dontSendNotification);

    if (! barsBox.isPopupActive())
        barsBox.setSelectedId (session.getPatternBars (current), juce::dontSendNotification);

    if (! swingSlider.isMouseButtonDown())
        swingSlider.setValue (session.getPatternSwing (current), juce::dontSendNotification);
}

//==============================================================================
bool RackView::isInterestedInDragSource (const SourceDetails& details)
{
    PluginBrowser::Entry e;
    juce::File f;

    return dynamic_cast<juce::FileTreeComponent*> (details.sourceComponent.get()) != nullptr
        || PluginBrowser::parseDrag (details.description, e)
        || KitBrowser::getDraggedFile (details.description, f);
}

void RackView::itemDropped (const SourceDetails& details)
{
    // A sound from Sounds search or favorites
    juce::File draggedFile;

    if (KitBrowser::getDraggedFile (details.description, draggedFile))
    {
        hovering = false;
        addSamples ({ draggedFile });
        repaint();
        return;
    }

    // A plugin from the Plugins browser
    PluginBrowser::Entry plugin;

    if (PluginBrowser::parseDrag (details.description, plugin))
    {
        hovering = false;
        repaint();

        // Instruments make a new row; effects go on the row they're dropped on
        if (plugin.instrument)
        {
            session.addInstrumentTrack (plugin.id, plugin.name);
            return;
        }

        const auto screenPos = localPointToGlobal (details.localPosition);

        for (auto* row : rows)
        {
            if (row->getScreenBounds().contains (screenPos))
            {
                for (int i = 0; i < session.getNumTracks(); ++i)
                    if (session.getTrack (i) == row->getTrack())
                        session.addEffectById (i, plugin.id);

                break;
            }
        }

        return;
    }

    hovering = false;

    if (auto* tree = dynamic_cast<juce::FileTreeComponent*> (details.sourceComponent.get()))
    {
        juce::Array<juce::File> files;

        for (int i = 0; i < tree->getNumSelectedFiles(); ++i)
            files.add (tree->getSelectedFile (i));

        addSamples (files);
    }

    repaint();
}

bool RackView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (isAudioFile (juce::File (f)))
            return true;

    return false;
}

void RackView::filesDropped (const juce::StringArray& files, int, int)
{
    hovering = false;

    juce::Array<juce::File> list;

    for (auto& f : files)
        list.add (juce::File (f));

    addSamples (list);
    repaint();
}

//==============================================================================
void RackView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d20));

    g.setColour (juce::Colour (0xff26292d));
    g.fillRect (getLocalBounds().removeFromTop (toolbarHeight));

    if (rows.isEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (juce::Font (juce::FontOptions (15.0f)));
        g.drawText ("Drag samples here from the Browser, or click + Add a sound or instrument.",
                    getLocalBounds().withTrimmedTop (toolbarHeight + addRowHeight + 12), juce::Justification::centred);
    }
}

void RackView::paintOverChildren (juce::Graphics& g)
{
    if (hovering)
    {
        g.setColour (juce::Colour (0xff3a8ee6).withAlpha (0.12f));
        g.fillRect (getLocalBounds().withTrimmedTop (toolbarHeight));
        g.setColour (juce::Colour (0xff3a8ee6));
        g.drawRect (getLocalBounds().withTrimmedTop (toolbarHeight), 2);
    }
}

void RackView::resized()
{
    auto tb = getLocalBounds().removeFromTop (toolbarHeight).reduced (6);

    patternBox.setBounds (tb.removeFromLeft (150));
    tb.removeFromLeft (6);
    barsBox.setBounds (tb.removeFromLeft (90));
    tb.removeFromLeft (16);
    swingLabel.setBounds (tb.removeFromLeft (46));
    swingSlider.setBounds (tb.removeFromLeft (180));

    viewport.setBounds (getLocalBounds().withTrimmedTop (toolbarHeight).reduced (6));
    layoutRows();
}
