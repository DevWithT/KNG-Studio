#include "TrackHeader.h"
#include "SliceEditorView.h"
#include "RoutePopup.h"
#include "KngTheme.h"

namespace te = tracktion;

// The lane color choices, used by the color strip and the right-click menu
static void addColourItems (juce::PopupMenu& menu, EditSession& session, int idx)
{
    struct Swatch { const char* name; juce::uint32 argb; };
    static const Swatch swatches[] = {
        { "Orange", 0xffc97d56 }, { "Yellow", 0xffc0a054 }, { "Blue",   0xff7c86c9 },
        { "Teal",   0xff5fa8a0 }, { "Green",  0xff809670 }, { "Pink",   0xffcd6e82 },
        { "Purple", 0xffa075b0 }, { "Brown",  0xffac8a6e }, { "Grey",   0xff7d848c }
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
    nameLabel.setFont (juce::Font (juce::FontOptions (13.5f, juce::Font::bold)));
    nameLabel.setColour (juce::Label::textColourId, KngTheme::ink);
    nameLabel.setColour (juce::Label::textWhenEditingColourId, KngTheme::text);
    nameLabel.setColour (juce::Label::backgroundWhenEditingColourId, KngTheme::inset);
    nameLabel.setColour (juce::Label::outlineWhenEditingColourId, KngTheme::accent);
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

    for (auto* b : { &armButton, &muteButton, &soloButton })
    {
        b->setColour (juce::TextButton::buttonColourId, juce::Colours::black.withAlpha (0.2f));
        b->setColour (juce::TextButton::textColourOffId, KngTheme::ink);
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    }

    armButton.setColour (juce::TextButton::buttonOnColourId, KngTheme::rec);
    muteButton.setColour (juce::TextButton::buttonOnColourId, KngTheme::ink);
    soloButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xfff2f2f2));
    soloButton.setColour (juce::TextButton::textColourOnId, KngTheme::ink);

    // Mon: hear this lane's input live while recording on it. Only shown while the lane is armed.
    addChildComponent (monButton);
    monButton.setClickingTogglesState (false);
    monButton.setColour (juce::TextButton::buttonColourId, juce::Colours::black.withAlpha (0.2f));
    monButton.setColour (juce::TextButton::textColourOffId, KngTheme::ink);
    monButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    monButton.setColour (juce::TextButton::buttonOnColourId, KngTheme::accent);
    monButton.setTooltip ("Monitor: hear this input live while recording on this lane");

    // Route: opens the Routing pop-up (output and sends)
    addChildComponent (routeButton);
    routeButton.setClickingTogglesState (false);
    routeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::black.withAlpha (0.2f));
    routeButton.setColour (juce::TextButton::textColourOffId, KngTheme::ink);
    routeButton.setColour (juce::TextButton::buttonOnColourId, KngTheme::accentSoft);
    routeButton.setColour (juce::TextButton::textColourOnId, KngTheme::text);
    routeButton.setTooltip ("Routing: where this lane's sound goes, and its sends");
    routeButton.onClick = [this] { RoutePopup::show (session, trackIndex, routeButton); };

    monButton.onClick = [this]
    {
        if (! session.getTransport().isRecording())
            session.setTrackMonitor (trackIndex, ! session.isTrackMonitor (trackIndex));
    };

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
    inputBox.setColour (juce::ComboBox::backgroundColourId, juce::Colours::black.withAlpha (0.2f));
    inputBox.setColour (juce::ComboBox::textColourId, KngTheme::ink);
    inputBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    inputBox.setColour (juce::ComboBox::arrowColourId, KngTheme::ink);
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

    const bool showMon = session.isArmed (trackIndex) && ! isBus;

    if (monButton.isVisible() != showMon)
    {
        monButton.setVisible (showMon);
        resized();
    }

    monButton.setToggleState (session.isTrackMonitor (trackIndex), juce::dontSendNotification);

    // Route lights up when this lane has sends or outputs to a bus
    const int numSends = session.getSendBuses (trackIndex).size();
    bool toBus = false;

    if (auto* routed = session.getTrack (trackIndex))
        toBus = routed->getOutput().getDestinationTrack() != nullptr;
    const auto routeText = juce::String ("Route") + (numSends > 0 ? " " + juce::String (numSends) : juce::String());

    if (routeButton.getButtonText() != routeText)
        routeButton.setButtonText (routeText);

    routeButton.setToggleState (numSends > 0 || toBus, juce::dontSendNotification);

    if (auto* t = session.getTrack (trackIndex))
    {
        const bool muted = t->isMuted (false);
        muteButton.setToggleState (muted, juce::dontSendNotification);
        soloButton.setToggleState (t->isSolo (false), juce::dontSendNotification);
        nameLabel.setColour (juce::Label::textColourId, KngTheme::ink.withAlpha (muted ? 0.6f : 0.95f));

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

    // The whole header takes the lane color (grey if the lane has none)
    auto base = isBus ? juce::Colour (0xff8c84a6) : KngTheme::laneGrey;
    bool muted = false;

    if (auto* t = session.getTrack (trackIndex))
    {
        const auto c = t->getColour();

        if (c.getAlpha() > 0)
            base = c.withAlpha (1.0f);

        muted = t->isMuted (false);
    }

    base = base.withMultipliedSaturation (0.78f);

    g.setColour (base.withAlpha (muted ? 0.5f : 1.0f));
    g.fillRoundedRectangle (r, KngTheme::radius);

    // Darker pill down the left edge (click it to pick a color)
    g.setColour (juce::Colours::black.withAlpha (0.2f));
    g.fillRoundedRectangle (r.getX() + 4.0f, r.getY() + 6.0f, 4.0f, r.getHeight() - 12.0f, 2.0f);

    if (session.isArmed (trackIndex))
    {
        g.setColour (KngTheme::rec);
        g.drawRoundedRectangle (r.reduced (1.0f), KngTheme::radius, 2.0f);
    }
    else if (session.getSelectedTrackIndex() == trackIndex)
    {
        // Selected lane: accent bar down the right edge
        g.setColour (KngTheme::accent);
        g.fillRoundedRectangle (r.getRight() - 4.0f, r.getY() + 6.0f, 3.0f, r.getHeight() - 12.0f, 1.5f);
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

    if (monButton.isVisible())
    {
        row1.removeFromRight (2);
        monButton.setBounds (row1.removeFromRight (34));
    }
    row1.removeFromRight (6);
    nameLabel.setBounds (row1);

    inputBox.setVisible (roomForInput);
    routeButton.setVisible (roomForInput);

    if (roomForInput)
    {
        r.removeFromTop (4);
        auto row2 = r.removeFromTop (22);
        routeButton.setBounds (row2.removeFromRight (66));
        row2.removeFromRight (4);
        inputBox.setBounds (row2);
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

    // Automate: pick what to automate and the lane opens on it, ready to draw
    {
        const auto targets = session.getAutomationTargets (idx);
        const auto current = session.getAutomationParamId (idx);
        const bool shown = session.isAutomationShown (idx);

        juce::PopupMenu automate;
        juce::StringArray pluginKeys, pluginNames;
        std::vector<juce::PopupMenu> pluginMenus;

        for (auto& tg : targets)
        {
            const juce::String id = tg.id;

            auto pick = [s, idx, id]
            {
                s->setAutomationParamId (idx, id);
                s->setAutomationShown (idx, true);
            };

            if (! id.startsWith ("p:"))
            {
                automate.addItem (tg.name, true, shown && id == current, pick);
                continue;
            }

            const auto key = id.upToLastOccurrenceOf (":", false, false);
            const int colon = tg.name.indexOf (": ");
            const auto pluginName = colon > 0 ? tg.name.substring (0, colon) : tg.name;
            const auto paramName = colon > 0 ? tg.name.substring (colon + 2) : tg.name;

            int slot = pluginKeys.indexOf (key);

            if (slot < 0)
            {
                pluginKeys.add (key);
                pluginNames.add (pluginName);
                pluginMenus.emplace_back();
                slot = pluginKeys.size() - 1;
            }

            pluginMenus[(size_t) slot].addItem (paramName, true, shown && id == current, pick);
        }

        if (! pluginKeys.isEmpty())
            automate.addSeparator();

        for (int k = 0; k < pluginKeys.size(); ++k)
            automate.addSubMenu (pluginNames[k], pluginMenus[(size_t) k]);

        if (! targets.empty())
            menu.addSubMenu ("Automate", automate);
    }

    menu.addSeparator();

    // KngTune
    menu.addItem ("Add KngTune", [s, idx]
    {
        if (! s->addKngTune (idx))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "KngTune not found",
                                                    "Put KngTune in your VST3 folder, then use File > Scan Plugins.");
    });

    // Routing: output and sends
    if (! isBus)
        menu.addItem ("Routing...", [s, idx, safe]
        {
            if (safe != nullptr)
                RoutePopup::show (*s, idx, *safe.getComponent());
        });

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

    // Slice Editor for one of this lane's audio clips
    {
        juce::PopupMenu slice;

        if (auto* laneTrack = s->getTrack (idx))
        {
            for (auto* c : laneTrack->getClips())
            {
                if (slice.getNumItems() >= 20)
                    break;

                if (auto* acb = dynamic_cast<te::WaveAudioClip*> (c))
                {
                    te::AudioClipBase::Ptr ptr (acb);
                    const auto label = (acb->getName().isNotEmpty() ? acb->getName() : juce::String ("Clip"))
                                       + "  (at " + juce::String (acb->getPosition().getStart().inSeconds(), 1) + " s)";

                    slice.addItem (label, [s, ptr] { showSliceEditor (*s, *ptr); });
                }
            }
        }

        if (slice.getNumItems() > 0)
            menu.addSubMenu ("Slice Editor", slice);
    }
    menu.addSeparator();

    // Deleting rebuilds the lane list, so do it after this header's click finishes
    menu.addItem ("Delete Lane", [s, idx]
    {
        juce::MessageManager::callAsync ([s, idx] { s->deleteTrack (idx); });
    });

    menu.showMenuAsync ({});
}
