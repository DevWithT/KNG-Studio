#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

/** The bar above a folder's tracks: collapse, name, mute and solo for the whole group. */
class FolderHeader : public juce::Component,
                     private juce::Timer
{
public:
    FolderHeader (EditSession& sessionToUse, const juce::String& folderName);
    ~FolderHeader() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Colour getColour() const;

    EditSession& session;
    juce::String folder;

    juce::TextButton arrowButton, muteButton { "M" }, soloButton { "S" };
    juce::Label nameLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FolderHeader)
};
