#include "FolderHeader.h"

FolderHeader::FolderHeader (EditSession& sessionToUse, const juce::String& folderName)
    : session (sessionToUse), folder (folderName)
{
    for (auto* c : std::initializer_list<juce::Component*> { &arrowButton, &nameLabel, &muteButton, &soloButton })
        addAndMakeVisible (c);

    nameLabel.setInterceptsMouseClicks (false, false);
    nameLabel.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));

    for (auto* b : { &muteButton, &soloButton })
        b->setClickingTogglesState (false);

    muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::orange);
    soloButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::gold);

    arrowButton.setTooltip ("Show or hide this folder's tracks");
    muteButton.setTooltip ("Mute every track in this folder");
    soloButton.setTooltip ("Solo every track in this folder");

    arrowButton.onClick = [this] { session.setFolderCollapsed (folder, ! session.isFolderCollapsed (folder)); };
    muteButton.onClick  = [this] { session.toggleFolderMute (folder); };
    soloButton.onClick  = [this] { session.toggleFolderSolo (folder); };

    timerCallback();
    startTimerHz (8);
}

FolderHeader::~FolderHeader()
{
    stopTimer();
}

juce::Colour FolderHeader::getColour() const
{
    const auto hue = (float) ((juce::uint32) folder.hashCode() % 360) / 360.0f;
    return juce::Colour::fromHSV (hue, 0.45f, 0.75f, 1.0f);
}

void FolderHeader::timerCallback()
{
    const auto members = session.getFolderTracks (folder);
    bool allMuted = ! members.isEmpty(), allSolo = ! members.isEmpty();

    for (int i : members)
    {
        if (auto* t = session.getTrack (i))
        {
            allMuted = allMuted && t->isMuted (false);
            allSolo = allSolo && t->isSolo (false);
        }
    }

    muteButton.setToggleState (allMuted, juce::dontSendNotification);
    soloButton.setToggleState (allSolo, juce::dontSendNotification);

    const bool collapsed = session.isFolderCollapsed (folder);
    arrowButton.setButtonText (juce::String (juce::CharPointer_UTF8 (collapsed ? "\xe2\x96\xb8" : "\xe2\x96\xbe")));

    const auto text = folder + "   (" + juce::String (members.size()) + (members.size() == 1 ? " track)" : " tracks)");

    if (nameLabel.getText() != text)
        nameLabel.setText (text, juce::dontSendNotification);
}

void FolderHeader::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff2c3035));
    g.fillRoundedRectangle (r, 4.0f);

    g.setColour (getColour());
    g.fillRoundedRectangle (r.withWidth (6.0f), 2.0f);
}

void FolderHeader::resized()
{
    auto r = getLocalBounds().reduced (6, 3);
    r.removeFromLeft (4);

    arrowButton.setBounds (r.removeFromLeft (26));
    r.removeFromLeft (6);
    soloButton.setBounds (r.removeFromRight (26));
    r.removeFromRight (3);
    muteButton.setBounds (r.removeFromRight (26));
    r.removeFromRight (8);
    nameLabel.setBounds (r);
}

void FolderHeader::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
    {
        session.setFolderCollapsed (folder, ! session.isFolderCollapsed (folder));
        return;
    }

    auto* s = &session;
    const auto name = folder;
    juce::PopupMenu menu;

    menu.addItem ("Rename...", [s, name]
    {
        auto* w = new juce::AlertWindow ("Rename Folder", "Name:", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", name);
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        w->enterModalState (true, juce::ModalCallbackFunction::create ([s, name, w] (int result)
        {
            const auto newName = w->getTextEditorContents ("name").trim();

            if (result == 1 && newName.isNotEmpty())
                s->renameFolder (name, newName);
        }), true);
    });

    menu.addItem ("Ungroup", [s, name] { s->removeFolder (name); });
    menu.showMenuAsync ({});
}
