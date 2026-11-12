#include "KngSamplerView.h"
#include "KngSamplerPlugin.h"
#include "TheoryHelper.h"
#include "KitBrowser.h"

namespace te = tracktion;

static bool isAudioFile (const juce::File& f)
{
    return f.hasFileExtension ("wav;aif;aiff;flac;mp3;ogg");
}

//==============================================================================
KngSamplerView::KngSamplerView (EditSession& sessionToUse, te::Track& trackToUse)
    : session (sessionToUse), track (&trackToUse)
{
    for (auto* c : std::initializer_list<juce::Component*> { &loadButton, &nameLabel, &tipLabel, &rootLabel, &rootBox,
                                                             &gainLabel, &gainSlider, &attackLabel, &attackSlider,
                                                             &releaseLabel, &releaseSlider, &glideLabel, &glideSlider,
                                                             &monoButton, &loopButton, &oneShotButton, &keyboard })
        addAndMakeVisible (c);

    loadButton.onClick = [this] { chooseFile(); };

    nameLabel.setFont (juce::Font (juce::FontOptions (15.0f)));

    tipLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    tipLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
    tipLabel.setText ("Root = the note your sample plays. For 808s: turn on Mono and Glide, then overlap two notes "
                      "in the piano roll and the second one slides. Sustain loop holds the tail for long notes.",
                      juce::dontSendNotification);

    for (auto* l : { &rootLabel, &gainLabel, &attackLabel, &releaseLabel, &glideLabel })
        l->setJustificationType (juce::Justification::centred);

    for (int note = 24; note <= 96; ++note)
        rootBox.addItem (Theory::noteName (note), note);

    rootBox.onChange = [this]
    {
        session.beginEdit();
        set (KngSamplerPlugin::rootProp, rootBox.getSelectedId());
        session.save();
    };

    setupKnob (gainSlider,    -24.0, 12.0,   0.0,  " dB", KngSamplerPlugin::gainProp);
    setupKnob (attackSlider,    0.0, 500.0,  40.0, " ms", KngSamplerPlugin::attackProp);
    setupKnob (releaseSlider,   1.0, 2000.0, 150.0, " ms", KngSamplerPlugin::releaseProp);
    setupKnob (glideSlider,     0.0, 500.0,  60.0, " ms", KngSamplerPlugin::glideProp);
    gainSlider.setDoubleClickReturnValue (true, 0.0);

    auto toggle = [this] (juce::ToggleButton& b, const juce::Identifier& id)
    {
        b.onClick = [this, &b, id]
        {
            session.beginEdit();
            set (id, b.getToggleState());
            session.save();
            repaint();
        };
    };

    toggle (monoButton, KngSamplerPlugin::monoProp);
    toggle (loopButton, KngSamplerPlugin::loopProp);
    toggle (oneShotButton, KngSamplerPlugin::oneShotProp);

    // Tooltips
    loadButton.setTooltip ("Load a sample from disk, or drag one in");
    rootBox.setTooltip ("The note your sample actually plays. Set this so it's in tune.");
    gainSlider.setTooltip ("Sample volume");
    attackSlider.setTooltip ("How fast the sound fades in");
    releaseSlider.setTooltip ("How long the sound rings after the note ends");
    glideSlider.setTooltip ("How long a slide takes when Mono is on and notes overlap");
    monoButton.setTooltip ("One note at a time; each new note cuts the last. Turn on for 808s.");
    loopButton.setTooltip ("Hold the sound for long notes by looping the orange region");
    oneShotButton.setTooltip ("Always play the whole sample, ignoring note length");

    keyboard.setAvailableRange (24, 96);
    keyboard.setLowestVisibleKey (36);
    keyboardState.addListener (this);

    syncControls();
    startTimerHz (10);
}

KngSamplerView::~KngSamplerView()
{
    stopTimer();
    keyboardState.removeListener (this);
}

void KngSamplerView::setupKnob (juce::Slider& s, double min, double max, double mid, const juce::String& suffix,
                                const juce::Identifier& id)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    s.setRange (min, max, 0.1);
    s.setSkewFactorFromMidPoint (mid);
    s.setTextValueSuffix (suffix);
    s.onDragStart   = [this] { session.beginEdit(); };
    s.onValueChange = [this, &s, id] { set (id, s.getValue()); };
    s.onDragEnd     = [this] { session.save(); };
}

//==============================================================================
int KngSamplerView::getTrackIndex()
{
    for (int i = 0; i < session.getNumTracks(); ++i)
        if (session.getTrack (i) == track.get())
            return i;

    return -1;
}

KngSamplerPlugin* KngSamplerView::getPlugin()
{
    return session.getKngSampler (getTrackIndex());
}

void KngSamplerView::set (const juce::Identifier& id, const juce::var& value)
{
    if (auto* k = getPlugin())
    {
        k->state.setProperty (id, value, &session.getEdit().getUndoManager());
        k->syncFromState();
    }
}

void KngSamplerView::loadFile (const juce::File& file)
{
    if (! isAudioFile (file))
        return;

    const int idx = getTrackIndex();
    session.setSamplerSample (idx, file, session.getSamplerRoot (idx));
    syncControls();
}

void KngSamplerView::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Load Sample", EditSession::getKitsFolder(),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              if (fc.getResult().existsAsFile())
                                  loadFile (fc.getResult());
                          });
}

void KngSamplerView::syncControls()
{
    auto* k = getPlugin();

    if (k == nullptr)
    {
        nameLabel.setText ("No KNG Sampler on this track. Drop a sample here to add one.", juce::dontSendNotification);
        return;
    }

    // Picks up undo and redo
    k->syncFromState();

    const auto& st = k->state;
    const auto name = k->getSampleFile().getFileNameWithoutExtension();
    nameLabel.setText (name.isNotEmpty() ? name : juce::String ("Drop a sample here: 808, keys, pluck, vocal"),
                       juce::dontSendNotification);

    if (! rootBox.isPopupActive())
        rootBox.setSelectedId ((int) st.getProperty (KngSamplerPlugin::rootProp, 60), juce::dontSendNotification);

    auto syncKnob = [] (juce::Slider& s, double v) { if (! s.isMouseButtonDown()) s.setValue (v, juce::dontSendNotification); };
    syncKnob (gainSlider,    (double) st.getProperty (KngSamplerPlugin::gainProp, 0.0));
    syncKnob (attackSlider,  (double) st.getProperty (KngSamplerPlugin::attackProp, 2.0));
    syncKnob (releaseSlider, (double) st.getProperty (KngSamplerPlugin::releaseProp, 30.0));
    syncKnob (glideSlider,   (double) st.getProperty (KngSamplerPlugin::glideProp, 0.0));

    monoButton.setToggleState ((bool) st.getProperty (KngSamplerPlugin::monoProp, false), juce::dontSendNotification);
    loopButton.setToggleState ((bool) st.getProperty (KngSamplerPlugin::loopProp, false), juce::dontSendNotification);
    oneShotButton.setToggleState ((bool) st.getProperty (KngSamplerPlugin::oneShotProp, false), juce::dontSendNotification);
}

void KngSamplerView::timerCallback()
{
    syncControls();

    if (auto* k = getPlugin())
        if (k->getSampleFile() != peaksFile || getWaveArea().getWidth() != peaksWidth)
            rebuildPeaks();

    repaint (getWaveArea());
}

void KngSamplerView::rebuildPeaks()
{
    peaks.clear();
    peaksWidth = getWaveArea().getWidth();

    auto* k = getPlugin();

    if (k == nullptr)
        return;

    peaksFile = k->getSampleFile();
    auto buffer = k->getSampleForDisplay();

    if (buffer == nullptr || peaksWidth <= 0)
        return;

    const int len = buffer->getNumSamples();
    const float* d = buffer->getReadPointer (0);
    peaks.resize ((size_t) peaksWidth, 0.0f);

    for (int x = 0; x < peaksWidth; ++x)
    {
        const int a = (int) ((juce::int64) x * len / peaksWidth);
        const int b = juce::jmax (a + 1, (int) ((juce::int64) (x + 1) * len / peaksWidth));
        float peak = 0.0f;

        for (int i = a; i < b && i < len; ++i)
            peak = juce::jmax (peak, std::abs (d[i]));

        peaks[(size_t) x] = peak;
    }
}

//==============================================================================
void KngSamplerView::handleNoteOn (juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity)
{
    if (auto* at = dynamic_cast<te::AudioTrack*> (track.get()))
        at->playGuideNote (midiNoteNumber, te::MidiChannel (1), juce::roundToInt (velocity * 127.0f), false, true, false);
}

void KngSamplerView::handleNoteOff (juce::MidiKeyboardState*, int, int, float)
{
    if (auto* at = dynamic_cast<te::AudioTrack*> (track.get()))
        at->turnOffGuideNotes (te::MidiChannel (1));
}

//==============================================================================
juce::Rectangle<int> KngSamplerView::getWaveArea() const
{
    return getLocalBounds().reduced (12).withTrimmedTop (36).withHeight (110);
}

int KngSamplerView::fracToX (float f) const
{
    auto w = getWaveArea();
    return w.getX() + juce::roundToInt (f * w.getWidth());
}

float KngSamplerView::xToFrac (int x) const
{
    auto w = getWaveArea();
    return juce::jlimit (0.0f, 1.0f, (x - w.getX()) / (float) juce::jmax (1, w.getWidth()));
}

void KngSamplerView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2124));

    auto wave = getWaveArea();

    g.setColour (hovering ? juce::Colour (0xff3a8ee6).withAlpha (0.2f) : juce::Colour (0xff17191b));
    g.fillRoundedRectangle (wave.toFloat(), 6.0f);

    if (hovering)
    {
        g.setColour (juce::Colour (0xff3a8ee6));
        g.drawRoundedRectangle (wave.toFloat(), 6.0f, 2.0f);
    }

    auto* k = getPlugin();

    if (peaks.empty() || k == nullptr)
    {
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawText ("Drop a sample here, or click Load Sample", wave, juce::Justification::centred);
        return;
    }

    const auto& st = k->state;
    const bool loopOn = (bool) st.getProperty (KngSamplerPlugin::loopProp, false);
    const float ls = (float) st.getProperty (KngSamplerPlugin::loopStartProp, 0.4f);
    const float le = (float) st.getProperty (KngSamplerPlugin::loopEndProp, 0.9f);

    // Loop region behind the waveform
    if (loopOn)
    {
        g.setColour (juce::Colour (0xffe6a23a).withAlpha (0.15f));
        g.fillRect (juce::Rectangle<int>::leftTopRightBottom (fracToX (ls), wave.getY(), fracToX (le), wave.getBottom()));
    }

    // Waveform
    g.setColour (juce::Colour (0xff3a8ee6));
    const float mid = (float) wave.getCentreY();
    const float half = wave.getHeight() * 0.45f;

    for (int x = 0; x < (int) peaks.size(); ++x)
    {
        const float h = juce::jmax (0.5f, peaks[(size_t) x] * half);
        g.drawVerticalLine (wave.getX() + x, mid - h, mid + h);
    }

    // Loop handles
    if (loopOn)
    {
        for (float f : { ls, le })
        {
            const int x = fracToX (f);
            g.setColour (juce::Colour (0xffe6a23a));
            g.fillRect (x - 1, wave.getY(), 3, wave.getHeight());
            g.fillRoundedRectangle ((float) x - 6.0f, (float) wave.getY(), 12.0f, 10.0f, 2.0f);
        }

        g.setColour (juce::Colour (0xffe6a23a));
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText ("Loop", fracToX (ls) + 8, wave.getY() + 12, 60, 14, juce::Justification::centredLeft);
    }
}

void KngSamplerView::resized()
{
    auto r = getLocalBounds().reduced (12);

    auto top = r.removeFromTop (28);
    loadButton.setBounds (top.removeFromLeft (130));
    top.removeFromLeft (10);
    nameLabel.setBounds (top);

    r.removeFromTop (8 + 110 + 10);

    // Settings row: root, four knobs, three toggles
    auto row = r.removeFromTop (92);

    auto rootCol = row.removeFromLeft (90);
    rootLabel.setBounds (rootCol.removeFromTop (18));
    rootBox.setBounds (rootCol.removeFromTop (26));
    row.removeFromLeft (10);

    auto knob = [&row] (juce::Label& l, juce::Slider& s)
    {
        auto col = row.removeFromLeft (82);
        l.setBounds (col.removeFromTop (18));
        s.setBounds (col);
        row.removeFromLeft (6);
    };

    knob (gainLabel, gainSlider);
    knob (attackLabel, attackSlider);
    knob (releaseLabel, releaseSlider);
    knob (glideLabel, glideSlider);

    row.removeFromLeft (10);
    auto toggles = row.removeFromLeft (180);
    monoButton.setBounds (toggles.removeFromTop (26));
    loopButton.setBounds (toggles.removeFromTop (26));
    oneShotButton.setBounds (toggles.removeFromTop (26));

    tipLabel.setBounds (row.reduced (10, 0));

    r.removeFromTop (6);
    keyboard.setBounds (r.withHeight (juce::jmax (60, r.getHeight())));
}

//==============================================================================
void KngSamplerView::mouseDown (const juce::MouseEvent& e)
{
    dragHandle = 0;
    auto* k = getPlugin();
    auto wave = getWaveArea();

    if (! wave.contains (e.getPosition()))
        return;

    if (k == nullptr || peaks.empty())
    {
        chooseFile();
        return;
    }

    if (! (bool) k->state.getProperty (KngSamplerPlugin::loopProp, false))
        return;

    const int xs = fracToX ((float) k->state.getProperty (KngSamplerPlugin::loopStartProp, 0.4f));
    const int xe = fracToX ((float) k->state.getProperty (KngSamplerPlugin::loopEndProp, 0.9f));

    if (std::abs (e.x - xs) <= 8)
        dragHandle = 1;
    else if (std::abs (e.x - xe) <= 8)
        dragHandle = 2;

    if (dragHandle != 0)
        session.beginEdit();
}

void KngSamplerView::mouseDrag (const juce::MouseEvent& e)
{
    auto* k = getPlugin();

    if (dragHandle == 0 || k == nullptr)
        return;

    const float f = xToFrac (e.x);
    const float ls = (float) k->state.getProperty (KngSamplerPlugin::loopStartProp, 0.4f);
    const float le = (float) k->state.getProperty (KngSamplerPlugin::loopEndProp, 0.9f);

    if (dragHandle == 1)
        set (KngSamplerPlugin::loopStartProp, juce::jlimit (0.0f, le - 0.02f, f));
    else
        set (KngSamplerPlugin::loopEndProp, juce::jlimit (ls + 0.02f, 1.0f, f));

    repaint (getWaveArea());
}

void KngSamplerView::mouseUp (const juce::MouseEvent&)
{
    if (dragHandle != 0)
        session.save();

    dragHandle = 0;
}

//==============================================================================
bool KngSamplerView::isInterestedInDragSource (const SourceDetails& details)
{
    juce::File f;

    return dynamic_cast<juce::FileTreeComponent*> (details.sourceComponent.get()) != nullptr
        || KitBrowser::getDraggedFile (details.description, f);
}

void KngSamplerView::itemDropped (const SourceDetails& details)
{
    hovering = false;
    juce::File f;

    if (KitBrowser::getDraggedFile (details.description, f))
        loadFile (f);
    else if (auto* tree = dynamic_cast<juce::FileTreeComponent*> (details.sourceComponent.get()))
        loadFile (tree->getSelectedFile());

    repaint();
}

bool KngSamplerView::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && isAudioFile (juce::File (files[0]));
}

void KngSamplerView::filesDropped (const juce::StringArray& files, int, int)
{
    hovering = false;

    if (! files.isEmpty())
        loadFile (juce::File (files[0]));

    repaint();
}
