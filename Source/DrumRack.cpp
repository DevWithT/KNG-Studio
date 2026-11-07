#include "DrumRack.h"
#include "StepSequencer.h"

namespace te = tracktion;

// Keyboard keys laid out like the 4x4 grid
static const juce::String padKeys = "1234QWERASDFZXCV";

static bool isAudioFile (const juce::File& f)
{
    return f.hasFileExtension ("wav;aif;aiff;flac;mp3;ogg");
}

//==============================================================================
DrumRackComponent::DrumRackComponent (EditSession& sessionToUse, te::Track& trackToUse)
    : session (sessionToUse), track (&trackToUse)
{
    setWantsKeyboardFocus (true);

    for (auto* c : std::initializer_list<juce::Component*> { &loadKitButton, &clearButton, &hintLabel, &padNameLabel,
                                                             &volLabel, &panLabel, &pitchLabel,
                                                             &volSlider, &panSlider, &pitchSlider })
        addAndMakeVisible (c);

    hintLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    hintLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    hintLabel.setText ("Drag a kit folder or samples onto the pads. Keys: 1234 QWER ASDF ZXCV",
                       juce::dontSendNotification);

    padNameLabel.setFont (juce::Font (juce::FontOptions (15.0f)));

    loadKitButton.onClick = [this] { loadKitFromChooser(); };
    clearButton.onClick = [this]
    {
        const int idx = getTrackIndex();

        for (int p = 0; p < 16; ++p)
            session.clearPad (idx, p);

        selectPad (selectedPad);
    };

    // Pad controls
    for (auto* sl : { &volSlider, &panSlider, &pitchSlider })
    {
        sl->setSliderStyle (juce::Slider::LinearHorizontal);
        sl->setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 22);
        sl->onDragStart = [this] { session.beginEdit(); };
        sl->onDragEnd   = [this] { session.save(); };
    }

    volSlider.setRange (-48.0, 12.0, 0.1);
    volSlider.setTextValueSuffix (" dB");
    volSlider.setDoubleClickReturnValue (true, 0.0);

    panSlider.setRange (-1.0, 1.0, 0.01);
    panSlider.setDoubleClickReturnValue (true, 0.0);

    pitchSlider.setRange (-24.0, 24.0, 1.0);
    pitchSlider.setTextValueSuffix (" st");
    pitchSlider.setDoubleClickReturnValue (true, 0.0);

    auto applyGains = [this]
    {
        session.setPadGainAndPan (getTrackIndex(), selectedPad,
                                  (float) volSlider.getValue(), (float) panSlider.getValue());
    };

    volSlider.onValueChange   = applyGains;
    panSlider.onValueChange   = applyGains;
    pitchSlider.onValueChange = [this] { session.setPadPitch (getTrackIndex(), selectedPad, (int) pitchSlider.getValue()); };

    selectPad (0);
    startTimerHz (30);
    setSize (520, 560);
}

DrumRackComponent::~DrumRackComponent()
{
    stopTimer();
}

int DrumRackComponent::getTrackIndex()
{
    for (int i = 0; i < session.getNumTracks(); ++i)
        if (session.getTrack (i) == track.get())
            return i;

    return -1;
}

//==============================================================================
juce::Rectangle<int> DrumRackComponent::getPadArea() const
{
    return getLocalBounds().withTrimmedTop (toolbarHeight).withTrimmedBottom (editorHeight).reduced (10);
}

juce::Rectangle<int> DrumRackComponent::getPadBounds (int pad) const
{
    auto area = getPadArea();
    const int w = area.getWidth() / 4;
    const int h = area.getHeight() / 4;
    return { area.getX() + (pad % 4) * w, area.getY() + (pad / 4) * h, w, h };
}

int DrumRackComponent::padAt (juce::Point<int> p) const
{
    for (int pad = 0; pad < 16; ++pad)
        if (getPadBounds (pad).contains (p))
            return pad;

    return -1;
}

//==============================================================================
void DrumRackComponent::triggerPad (int pad)
{
    hitTimes[pad] = juce::Time::getMillisecondCounter();
    session.playPad (getTrackIndex(), pad, 110);
    repaint();
}

void DrumRackComponent::selectPad (int pad)
{
    selectedPad = pad;
    const int idx = getTrackIndex();

    const auto name = session.getPadName (idx, pad);
    padNameLabel.setText ("Pad " + juce::String (pad + 1) + (name.isNotEmpty() ? ": " + name : juce::String (" (empty)")),
                          juce::dontSendNotification);

    volSlider.setValue (session.getPadGain (idx, pad), juce::dontSendNotification);
    panSlider.setValue (session.getPadPan (idx, pad), juce::dontSendNotification);
    pitchSlider.setValue (session.getPadPitch (idx, pad), juce::dontSendNotification);

    repaint();
}

void DrumRackComponent::dropFiles (const juce::Array<juce::File>& files, int pad)
{
    const int idx = getTrackIndex();

    if (files.isEmpty() || idx < 0)
        return;

    // A folder fills the whole rack
    if (files.size() == 1 && files.getFirst().isDirectory())
    {
        session.loadKitFolder (idx, files.getFirst());
        selectPad (0);
        return;
    }

    // Samples fill pads in order, starting where you dropped them
    int p = pad >= 0 ? pad : selectedPad;

    for (auto& f : files)
    {
        if (p > 15)
            break;

        if (isAudioFile (f) && session.setPadSample (idx, p, f))
            ++p;
    }

    selectPad (pad >= 0 ? pad : selectedPad);
}

void DrumRackComponent::loadKitFromChooser()
{
    chooser = std::make_unique<juce::FileChooser> ("Load Kit Folder", EditSession::getKitsFolder());

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto folder = fc.getResult();

                              if (folder.isDirectory())
                                  dropFiles ({ folder }, 0);
                          });
}

void DrumRackComponent::showPadMenu (int pad)
{
    juce::Component::SafePointer<DrumRackComponent> safe (this);
    juce::PopupMenu menu;

    menu.addItem ("Load Sample...", [safe, pad]
    {
        if (safe == nullptr)
            return;

        safe->chooser = std::make_unique<juce::FileChooser> ("Load Sample", EditSession::getKitsFolder(),
                                                             "*.wav;*.aif;*.aiff;*.flac");
        safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                    [safe, pad] (const juce::FileChooser& fc)
                                    {
                                        if (safe != nullptr && fc.getResult().existsAsFile())
                                            safe->dropFiles ({ fc.getResult() }, pad);
                                    });
    });

    menu.addItem ("Rename...", [safe, pad]
    {
        if (safe == nullptr)
            return;

        auto* w = new juce::AlertWindow ("Rename Pad", "Name:", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", safe->session.getPadName (safe->getTrackIndex(), pad));
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, pad, w] (int result)
        {
            if (safe != nullptr && result == 1)
            {
                safe->session.setPadName (safe->getTrackIndex(), pad, w->getTextEditorContents ("name").trim());
                safe->selectPad (pad);
            }
        }), true);
    });

    menu.addItem ("Clear Pad", [safe, pad]
    {
        if (safe != nullptr)
        {
            safe->session.clearPad (safe->getTrackIndex(), pad);
            safe->selectPad (pad);
        }
    });

    menu.showMenuAsync ({});
}

//==============================================================================
void DrumRackComponent::mouseDown (const juce::MouseEvent& e)
{
    const int pad = padAt (e.getPosition());

    if (pad < 0)
        return;

    grabKeyboardFocus();
    selectPad (pad);

    if (e.mods.isPopupMenu())
        showPadMenu (pad);
    else
        triggerPad (pad);
}

bool DrumRackComponent::keyPressed (const juce::KeyPress& key)
{
    if (key.getModifiers().isAnyModifierKeyDown())
        return false;

    const int pad = padKeys.indexOfChar (juce::CharacterFunctions::toUpperCase (key.getTextCharacter()));

    if (pad >= 0)
    {
        triggerPad (pad);
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        if (session.getTransport().isPlaying())
            session.stop();
        else
            session.play();

        return true;
    }

    return false;
}

//==============================================================================
bool DrumRackComponent::isInterestedInDragSource (const SourceDetails& details)
{
    return dynamic_cast<juce::FileTreeComponent*> (details.sourceComponent.get()) != nullptr;
}

void DrumRackComponent::itemDragMove (const SourceDetails& details)
{
    hoverPad = padAt (details.localPosition);
}

void DrumRackComponent::itemDragExit (const SourceDetails&)
{
    hoverPad = -1;
}

void DrumRackComponent::itemDropped (const SourceDetails& details)
{
    hoverPad = -1;

    if (auto* tree = dynamic_cast<juce::FileTreeComponent*> (details.sourceComponent.get()))
        dropFiles ({ tree->getSelectedFile() }, padAt (details.localPosition));
}

bool DrumRackComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& path : files)
    {
        juce::File f (path);

        if (f.isDirectory() || isAudioFile (f))
            return true;
    }

    return false;
}

void DrumRackComponent::fileDragMove (const juce::StringArray&, int x, int y)
{
    hoverPad = padAt ({ x, y });
}

void DrumRackComponent::fileDragExit (const juce::StringArray&)
{
    hoverPad = -1;
}

void DrumRackComponent::filesDropped (const juce::StringArray& paths, int x, int y)
{
    hoverPad = -1;

    juce::Array<juce::File> files;

    for (auto& path : paths)
        files.add (juce::File (path));

    dropFiles (files, padAt ({ x, y }));
}

//==============================================================================
void DrumRackComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2124));

    g.setColour (juce::Colour (0xff2b2f33));
    g.fillRect (getLocalBounds().removeFromTop (toolbarHeight));
    g.fillRect (getLocalBounds().removeFromBottom (editorHeight));

    const int idx = getTrackIndex();
    const auto now = juce::Time::getMillisecondCounter();

    for (int pad = 0; pad < 16; ++pad)
    {
        auto r = getPadBounds (pad).reduced (4).toFloat();
        const auto name = session.getPadName (idx, pad);
        const bool flashing = now - hitTimes[pad] < 140;

        g.setColour (flashing          ? juce::Colour (0xff6fb0ff)
                   : name.isNotEmpty() ? juce::Colour (0xff3a4a5c)
                                       : juce::Colour (0xff2e3236));
        g.fillRoundedRectangle (r, 6.0f);

        if (pad == hoverPad)
        {
            g.setColour (juce::Colour (0xff3a8ee6));
            g.drawRoundedRectangle (r, 6.0f, 3.0f);
        }
        else if (pad == selectedPad)
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (r, 6.0f, 2.0f);
        }

        // Key letter in the corner
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (padKeys.substring (pad, pad + 1), r.reduced (6.0f).toNearestInt(), juce::Justification::topLeft);

        // Sample name
        g.setColour (juce::Colours::white.withAlpha (name.isNotEmpty() ? 0.95f : 0.35f));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawFittedText (name.isNotEmpty() ? name : "Pad " + juce::String (pad + 1),
                          r.reduced (6.0f).toNearestInt(), juce::Justification::centred, 3);
    }
}

void DrumRackComponent::resized()
{
    auto top = getLocalBounds().removeFromTop (toolbarHeight).reduced (6);
    loadKitButton.setBounds (top.removeFromLeft (100));
    top.removeFromLeft (6);
    clearButton.setBounds (top.removeFromLeft (90));
    top.removeFromLeft (10);
    hintLabel.setBounds (top);

    auto ed = getLocalBounds().removeFromBottom (editorHeight).reduced (10, 6);
    padNameLabel.setBounds (ed.removeFromTop (22));
    ed.removeFromTop (4);

    auto row = [&ed] { auto x = ed.removeFromTop (24); ed.removeFromTop (4); return x; };

    auto r1 = row();
    volLabel.setBounds (r1.removeFromLeft (60));
    volSlider.setBounds (r1);

    auto r2 = row();
    panLabel.setBounds (r2.removeFromLeft (60));
    panSlider.setBounds (r2);

    auto r3 = row();
    pitchLabel.setBounds (r3.removeFromLeft (60));
    pitchSlider.setBounds (r3);
}

//==============================================================================
namespace
{
    // The drum window: Sequencer and Pads as tabs
    struct DrumRackTabs : public juce::Component
    {
        DrumRackTabs (EditSession& session, te::Track& t)
        {
            tabs.setTabBarDepth (30);
            tabs.addTab ("Sequencer", juce::Colour (0xff2b2f33), new StepSequencer (session, t), true);
            tabs.addTab ("Pads",      juce::Colour (0xff2b2f33), new DrumRackComponent (session, t), true);
            addAndMakeVisible (tabs);
            setSize (920, 620);
        }

        void resized() override   { tabs.setBounds (getLocalBounds()); }

        juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    };
}

DrumRackWindow::DrumRackWindow (EditSession& session, te::Track& t)
    : DocumentWindow ("Drums - " + t.getName(), juce::Colour (0xff1e2124), DocumentWindow::allButtons),
      track (&t)
{
    setUsingNativeTitleBar (true);
    setContentOwned (new DrumRackTabs (session, t), true);
    setResizable (true, false);
    centreWithSize (920, 620);
    setVisible (true);
}

std::unique_ptr<juce::Component> createDrumRackPanel (EditSession& session, te::Track& track)
{
    return std::make_unique<DrumRackTabs> (session, track);
}
