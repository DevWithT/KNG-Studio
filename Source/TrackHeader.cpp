#include "TrackHeader.h"
#include "SendPopup.h"

namespace te = tracktion;

// The lane color choices, used by the color strip and the right-click menu
static void addColourItems (juce::PopupMenu& menu, EditSession& session, int idx)
{
    struct Swatch { const char* name; juce::uint32 argb; };
    static const Swatch swatches[] = {
        { "Blue",   0xff3a6ea5 }, { "Red",    0xffc0392b }, { "Orange", 0xffd35400 },
        { "Yellow", 0xffc9a227 }, { "Green",  0xff2e8b57 }, { "Teal",   0xff16a085 },
        { "Purple", 0xff8e44ad }, { "Pink",   0xffc2185b }, { "Grey",   0xff607d8b }
    };

    auto* s = &session;

    for (auto& sw : swatches)
    {
        const juce::Colour c (sw.argb);
        juce::PopupMenu::Item item (sw.name);
        item.colour = c;
        item.action = [s, idx, c]
        {
            if (auto* t = s->getTrack (idx))
            {
                s->beginEdit();
                t->setColour (c);
                s->save();
            }
        };
        menu.addItem (std::move (item));
    }
}

// Moves a lane past any hidden instrument tracks so it visibly moves one lane
static void moveLane (EditSession* s, int idx, int direction)
{
    int target = idx + direction;

    while (target >= 0 && target < s->getNumTracks() && ! s->isLaneTrack (target))
        target += direction;

    if (target < 0 || target >= s->getNumTracks())
        return;

    for (int at = idx; at != target; at += direction)
        s->moveTrack (at, direction);
}

//==============================================================================
TrackHeader::TrackHeader (EditSession& sessionToUse, int trackIndexToUse)
    : session (sessionToUse),
      trackIndex (trackIndexToUse),
      isBus (sessionToUse.isReverbBus (trackIndexToUse))
{
    addAndMakeVisible (nameLabel);
    nameLabel.setFont (juce::Font (juce::FontOptions (14.0f)));
    nameLabel.setMinimumHorizontalScale (0.8f);
    nameLabel.setInterceptsMouseClicks (false, false);
    nameLabel.onTextChange = [this]
    {
        if (auto* t = session.getTrack (trackIndex))
        {
            const auto newName = nameLabel.getText().trim();

            if (newName.isNotEmpty())
            {
                session.beginEdit();
                t->setName (newName);
                session.save();
            }
        }
    };

    for (auto* b : { &armButton, &muteButton, &soloButton })
    {
        addAndMakeVisible (b);
        b->setClickingTogglesState (false);
    }

    armButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::red);
    muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::orange);
    soloButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::gold);

    armButton.onClick = [this] { session.setArmed (trackIndex, ! session.isArmed (trackIndex)); };

    muteButton.onClick = [this]
    {
        if (auto* t = session.getTrack (trackIndex))
            t->setMute (! t->isMuted (false));
    };

    soloButton.onClick = [this]
    {
        if (auto* t = session.getTrack (trackIndex))
            t->setSolo (! t->isSolo (false));
    };

    addChildComponent (inputBox);
    inputBox.setTextWhenNoChoicesAvailable ("No inputs");
    inputBox.onChange = [this]
    {
        const int id = inputBox.getSelectedId();

        if (id > 0)
            session.setTrackInput (trackIndex, id - 1);
    };
    refreshInputList();

    addAndMakeVisible (meter);

    if (auto* t = session.getTrack (trackIndex))
    {
        nameLabel.setText (t->getName(), juce::dontSendNotification);
        meter.attachTo (t->getLevelMeterPlugin());
    }

    // Tooltips
    armButton.setTooltip ("Arm this lane for recording");
    muteButton.setTooltip ("Mute this lane, its audio and its pattern blocks");
    soloButton.setTooltip ("Solo: hear only this lane");
    inputBox.setTooltip ("Which input this lane records from");

    // Buses don't record
    if (isBus)
        armButton.setVisible (false);

    startTimerHz (10);
}

TrackHeader::~TrackHeader()
{
    stopTimer();
}

void TrackHeader::refreshInputList()
{
    auto names = session.getInputNames (trackIndex);

    if (names == lastInputNames)
        return;

    lastInputNames = names;
    inputBox.clear (juce::dontSendNotification);

    for (int i = 0; i < names.size(); ++i)
        inputBox.addItem (names[i], i + 1);

    if (names.size() > 0)
        inputBox.setSelectedId (session.getTrackInput (trackIndex) + 1, juce::dontSendNotification);
}

void TrackHeader::timerCallback()
{
    refreshInputList();

    armButton.setToggleState (session.isArmed (trackIndex), juce::dontSendNotification);

    if (auto* t = session.getTrack (trackIndex))
    {
        const bool muted = t->isMuted (false);
        muteButton.setToggleState (muted, juce::dontSendNotification);
        soloButton.setToggleState (t->isSolo (false), juce::dontSendNotification);
        nameLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (muted ? 0.45f : 0.92f));

        // Picks up undo/redo of a rename
        if (! nameLabel.isBeingEdited() && nameLabel.getText() != t->getName())
            nameLabel.setText (t->getName(), juce::dontSendNotification);
    }

    if (! inputBox.isPopupActive() && inputBox.getNumItems() > 0)
        inputBox.setSelectedId (session.getTrackInput (trackIndex) + 1, juce::dontSendNotification);

    repaint();
}

void TrackHeader::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    g.setColour (isBus ? juce::Colour (0xff3b3548) : juce::Colour (0xff3a3f44));
    g.fillRoundedRectangle (r, 4.0f);

    // Color strip down the left edge (click it to pick a color)
    if (auto* t = session.getTrack (trackIndex))
    {
        const auto c = t->getColour();
        g.setColour (c.getAlpha() > 0 ? c.withAlpha (1.0f) : juce::Colour (0xff5a6066));
        g.fillRoundedRectangle (r.withWidth ((float) stripWidth), 2.0f);
    }

    if (session.isArmed (trackIndex))
    {
        g.setColour (juce::Colours::red.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 2.0f);
    }
    else if (session.getSelectedTrackIndex() == trackIndex)
    {
        g.setColour (juce::Colour (0xff3a8ee6));
        g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 2.0f);
    }
}

void TrackHeader::resized()
{
    auto r = getLocalBounds().reduced (5, 4);
    r.removeFromLeft (stripWidth + 3);

    meter.setBounds (r.removeFromRight (6));
    r.removeFromRight (4);

    // Short lanes: one centered row. Taller lanes: the input goes underneath.
    const bool roomForInput = ! isBus && r.getHeight() >= 48;
    auto row1 = roomForInput ? r.removeFromTop (22)
                             : r.withSizeKeepingCentre (r.getWidth(), juce::jmin (22, r.getHeight()));

    soloButton.setBounds (row1.removeFromRight (24));
    row1.removeFromRight (2);
    muteButton.setBounds (row1.removeFromRight (24));
    row1.removeFromRight (2);
    armButton.setBounds (row1.removeFromRight (24));
    row1.removeFromRight (6);
    nameLabel.setBounds (row1);

    inputBox.setVisible (roomForInput);

    if (roomForInput)
    {
        r.removeFromTop (4);
        inputBox.setBounds (r.removeFromTop (22));
    }
}

void TrackHeader::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (nameLabel.getBounds().contains (e.getPosition()))
        nameLabel.showEditor();
}

void TrackHeader::mouseDown (const juce::MouseEvent& e)
{
    // Clicking a lane shows it in the inspector on the right
    session.selectTrack (trackIndex);

    if (! e.mods.isPopupMenu())
    {
        // The color strip picks a color
        if (e.x <= stripWidth + 6)
        {
            juce::PopupMenu colours;
            addColourItems (colours, session, trackIndex);
            colours.showMenuAsync ({});
        }

        return;
    }

    auto* s = &session;
    const int idx = trackIndex;
    juce::Component::SafePointer<TrackHeader> safe (this);

    juce::PopupMenu menu;

    menu.addItem ("Rename", [safe] { if (safe != nullptr) safe->nameLabel.showEditor(); });

    menu.addItem ("Move Up", true, false,
                  [s, idx] { juce::MessageManager::callAsync ([s, idx] { moveLane (s, idx, -1); }); });

    menu.addItem ("Move Down", true, false,
                  [s, idx] { juce::MessageManager::callAsync ([s, idx] { moveLane (s, idx, 1); }); });

    juce::PopupMenu colours;
    addColourItems (colours, session, idx);
    menu.addSubMenu ("Color", colours);

    // Input (handy when the lane is too short to show the input box)
    if (! isBus)
    {
        juce::PopupMenu inputs;
        const auto names = session.getInputNames (idx);
        const int current = session.getTrackInput (idx);

        for (int i = 0; i < names.size(); ++i)
            inputs.addItem (names[i], true, i == current, [s, idx, i] { s->setTrackInput (idx, i); });

        if (names.size() > 0)
            menu.addSubMenu ("Input", inputs);
    }

    menu.addItem (session.isAutomationShown (idx) ? "Hide Automation" : "Show Automation",
                  [s, idx] { s->setAutomationShown (idx, ! s->isAutomationShown (idx)); });

    menu.addSeparator();

    // KngTune
    menu.addItem ("Add KngTune", [s, idx]
    {
        if (! s->addKngTune (idx))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "KngTune not found",
                                                    "Put KngTune in your VST3 folder, then use File > Scan Plugins.");
    });

    // Bounce the lane (with its effects) to audio on a new lane
    menu.addItem ("Bounce in Place", [s, idx] { s->bounceTrackInPlace (idx); });

    // Overlap two clips, then this blends them so there's no click or jump at the join
    menu.addItem ("Crossfade Overlapping Clips", [s, idx]
    {
        if (s->crossfadeOverlaps (idx) == 0)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "No overlaps",
                                                    "Drag one clip so its start overlaps the end of the one before it, then try again.");
    });

    // Sends to buses
    if (! isBus)
    {
        juce::PopupMenu sends;

        for (int bus : session.getBusNumbers())
        {
            const float db = session.getSendToBusDb (idx, bus);
            const auto level = db > -60.0f ? "   (" + juce::String (db, 1) + " dB)" : juce::String();

            sends.addItem (session.getBusName (bus) + level, [s, idx, bus, safe]
            {
                if (safe != nullptr)
                    SendPopup::show (*s, idx, bus, *safe.getComponent());
            });
        }

        sends.addSeparator();
        sends.addItem ("New Bus...", [s, idx]
        {
            auto* w = new juce::AlertWindow ("New Bus", "Name this bus:", juce::MessageBoxIconType::NoIcon);
            w->addTextEditor ("name", "Delay Bus");
            w->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

            w->enterModalState (true, juce::ModalCallbackFunction::create ([s, idx, w] (int result)
            {
                const auto name = w->getTextEditorContents ("name").trim();

                if (result == 1 && name.isNotEmpty())
                {
                    const int bus = s->addBus (name);

                    if (bus >= 0)
                        s->setSendToBusDb (idx, bus, -6.0f);
                }
            }), true);
        });

        menu.addSubMenu ("Sends", sends);
    }

    // Sidechain ducking: this lane gets quieter when the picked track plays
    juce::PopupMenu duck;
    const int duckSource = session.getDuckSource (idx);

    for (int i = 0; i < session.getNumTracks(); ++i)
        if (i != idx && ! session.isReverbBus (i))
            if (auto* t = session.getTrack (i))
                duck.addItem (t->getName(), true, i == duckSource, [s, idx, i] { s->duckFromTrack (idx, i); });

    if (duckSource >= 0)
    {
        duck.addSeparator();
        duck.addItem ("Remove Ducking", [s, idx] { s->removeDucking (idx); });
    }

    menu.addSubMenu ("Sidechain Duck From", duck);

    // Folders
    juce::PopupMenu folders;
    const auto currentFolder = session.getTrackFolder (idx);

    folders.addItem ("New Folder...", [s, idx]
    {
        auto* w = new juce::AlertWindow ("New Folder", "Name this folder:", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", "Drums");
        w->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        w->enterModalState (true, juce::ModalCallbackFunction::create ([s, idx, w] (int result)
        {
            const auto name = w->getTextEditorContents ("name").trim();

            if (result == 1 && name.isNotEmpty())
                s->setTrackFolder (idx, name);
        }), true);
    });

    for (auto& f : session.getFolderNames())
        if (f != currentFolder)
            folders.addItem ("Move to " + f, [s, idx, f]
            {
                juce::MessageManager::callAsync ([s, idx, f] { s->setTrackFolder (idx, f); });
            });

    if (currentFolder.isNotEmpty())
        folders.addItem ("Remove from " + currentFolder, [s, idx] { s->setTrackFolder (idx, {}); });

    menu.addSubMenu ("Folder", folders);

    // Chain templates
    juce::PopupMenu templates;

    templates.addItem ("Save Chain as Template...", [s, idx]
    {
        auto* w = new juce::AlertWindow ("Save Template", "Name this FX chain:", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", "Vocal Chain");
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        w->enterModalState (true, juce::ModalCallbackFunction::create ([s, idx, w] (int result)
        {
            if (result == 1)
                s->saveChainTemplate (idx, w->getTextEditorContents ("name"));
        }), true);
    });

    const auto templateNames = session.getTemplateNames();

    if (! templateNames.isEmpty())
        templates.addSeparator();

    for (auto& name : templateNames)
        templates.addItem (name, [s, idx, name] { s->loadChainTemplate (idx, name); });

    menu.addSubMenu ("Templates", templates);
    menu.addSeparator();

    // Deleting rebuilds the lane list, so do it after this header's click finishes
    menu.addItem ("Delete Lane", [s, idx]
    {
        juce::MessageManager::callAsync ([s, idx] { s->deleteTrack (idx); });
    });

    menu.showMenuAsync ({});
}
