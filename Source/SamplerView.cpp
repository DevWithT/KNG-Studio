#include "SamplerView.h"
#include "TheoryHelper.h"

namespace te = tracktion;

static bool isAudioFile (const juce::File& f)
{
    return f.hasFileExtension ("wav;aif;aiff;flac;mp3;ogg");
}

//==============================================================================
SamplerComponent::SamplerComponent (EditSession& sessionToUse, te::Track& trackToUse)
    : session (sessionToUse), track (&trackToUse)
{
    for (auto* c : std::initializer_list<juce::Component*> { &loadButton, &sampleLabel, &tipLabel, &rootLabel, &rootBox,
                                                             &gainLabel, &gainSlider, &oneShotButton, &keyboard })
        addAndMakeVisible (c);

    sampleLabel.setJustificationType (juce::Justification::centred);
    sampleLabel.setFont (juce::Font (juce::FontOptions (15.0f)));
    sampleLabel.setInterceptsMouseClicks (false, false);

    tipLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    tipLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
    tipLabel.setText ("Root note = the note the sample actually plays. An 808 tuned to C should be set to C. "
                      "Then play it in the piano roll like any instrument.",
                      juce::dontSendNotification);

    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load Sample", EditSession::getKitsFolder(),
                                                       "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile())
                                      loadFile (fc.getResult());
                              });
    };

    // Root note, C1 to C7
    for (int note = 24; note <= 96; ++note)
        rootBox.addItem (Theory::noteName (note), note);

    rootBox.onChange = [this]
    {
        session.beginEdit();
        session.setSamplerRoot (getTrackIndex(), rootBox.getSelectedId());
        session.save();
    };

    gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
    gainSlider.setRange (-48.0, 12.0, 0.1);
    gainSlider.setTextValueSuffix (" dB");
    gainSlider.setDoubleClickReturnValue (true, 0.0);
    gainSlider.onDragStart   = [this] { session.beginEdit(); };
    gainSlider.onValueChange = [this] { session.setSamplerGain (getTrackIndex(), (float) gainSlider.getValue()); };
    gainSlider.onDragEnd     = [this] { session.save(); };

    oneShotButton.onClick = [this]
    {
        session.beginEdit();
        session.setSamplerOneShot (getTrackIndex(), oneShotButton.getToggleState());
        session.save();
    };

    keyboard.setAvailableRange (24, 96);
    keyboard.setLowestVisibleKey (36);
    keyboardState.addListener (this);

    syncFromSampler();
    startTimerHz (4);
    setSize (680, 380);
}

SamplerComponent::~SamplerComponent()
{
    stopTimer();
    keyboardState.removeListener (this);
}

int SamplerComponent::getTrackIndex()
{
    for (int i = 0; i < session.getNumTracks(); ++i)
        if (session.getTrack (i) == track.get())
            return i;

    return -1;
}

void SamplerComponent::loadFile (const juce::File& file)
{
    if (isAudioFile (file))
        session.setSamplerSample (getTrackIndex(), file, rootBox.getSelectedId() > 0 ? rootBox.getSelectedId() : 60);

    syncFromSampler();
}

void SamplerComponent::syncFromSampler()
{
    const int idx = getTrackIndex();
    const auto name = session.getSamplerSampleName (idx);

    sampleLabel.setText (name.isNotEmpty() ? name : juce::String ("Drop a sample here: 808, keys, pluck, vocal, anything"),
                         juce::dontSendNotification);

    if (! rootBox.isPopupActive())
        rootBox.setSelectedId (session.getSamplerRoot (idx), juce::dontSendNotification);

    if (! gainSlider.isMouseButtonDown())
        gainSlider.setValue (session.getSamplerGain (idx), juce::dontSendNotification);

    oneShotButton.setToggleState (session.getSamplerOneShot (idx), juce::dontSendNotification);
}

//==============================================================================
void SamplerComponent::handleNoteOn (juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity)
{
    if (auto* at = dynamic_cast<te::AudioTrack*> (track.get()))
        at->playGuideNote (midiNoteNumber, te::MidiChannel (1), juce::roundToInt (velocity * 127.0f), false, true, false);
}

void SamplerComponent::handleNoteOff (juce::MidiKeyboardState*, int, int, float)
{
    if (auto* at = dynamic_cast<te::AudioTrack*> (track.get()))
        at->turnOffGuideNotes (te::MidiChannel (1));
}

//==============================================================================
bool SamplerComponent::isInterestedInDragSource (const SourceDetails& details)
{
    return dynamic_cast<juce::FileTreeComponent*> (details.sourceComponent.get()) != nullptr;
}

void SamplerComponent::itemDropped (const SourceDetails& details)
{
    hovering = false;

    if (auto* tree = dynamic_cast<juce::FileTreeComponent*> (details.sourceComponent.get()))
        loadFile (tree->getSelectedFile());

    repaint();
}

bool SamplerComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && isAudioFile (juce::File (files[0]));
}

void SamplerComponent::filesDropped (const juce::StringArray& files, int, int)
{
    hovering = false;

    if (! files.isEmpty())
        loadFile (juce::File (files[0]));

    repaint();
}

//==============================================================================
juce::Rectangle<int> SamplerComponent::getDropArea() const
{
    return getLocalBounds().reduced (12).withTrimmedTop (40).withHeight (100);
}

void SamplerComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2124));

    auto drop = getDropArea().toFloat();

    g.setColour (hovering ? juce::Colour (0xff3a8ee6).withAlpha (0.25f) : juce::Colour (0xff2b2f33));
    g.fillRoundedRectangle (drop, 8.0f);

    juce::Path border;
    border.addRoundedRectangle (drop.reduced (1.0f), 8.0f);

    const float dashes[] = { 6.0f, 4.0f };
    juce::Path dashed;
    juce::PathStrokeType (1.5f).createDashedStroke (dashed, border, dashes, 2);

    g.setColour (hovering ? juce::Colour (0xff3a8ee6) : juce::Colours::white.withAlpha (0.25f));
    g.fillPath (dashed);
}

void SamplerComponent::resized()
{
    auto r = getLocalBounds().reduced (12);

    loadButton.setBounds (r.removeFromTop (28).removeFromLeft (130));
    r.removeFromTop (12);

    sampleLabel.setBounds (getDropArea());
    r.removeFromTop (100);
    r.removeFromTop (12);

    auto row = r.removeFromTop (26);
    rootLabel.setBounds (row.removeFromLeft (80));
    rootBox.setBounds (row.removeFromLeft (90));
    row.removeFromLeft (20);
    gainLabel.setBounds (row.removeFromLeft (60));
    gainSlider.setBounds (row.removeFromLeft (220));

    r.removeFromTop (6);
    oneShotButton.setBounds (r.removeFromTop (24).removeFromLeft (300));

    keyboard.setBounds (r.removeFromBottom (80));
    r.removeFromBottom (6);
    tipLabel.setBounds (r);
}

//==============================================================================
SamplerWindow::SamplerWindow (EditSession& session, te::Track& t)
    : DocumentWindow ("KNG Sampler - " + t.getName(), juce::Colour (0xff1e2124), DocumentWindow::allButtons),
      track (&t)
{
    setUsingNativeTitleBar (true);
    setContentOwned (new SamplerComponent (session, t), true);
    setResizable (true, false);
    centreWithSize (680, 380);
    setVisible (true);
}
