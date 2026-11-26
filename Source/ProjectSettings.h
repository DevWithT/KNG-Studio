#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"
#include "KngTheme.h"

// File > Project Settings: the song-wide switches in one place.
class ProjectSettings : public juce::Component
{
public:
    explicit ProjectSettings (EditSession& s) : session (s)
    {
        addAndMakeVisible (nameLabel);
        nameLabel.setText (session.getProjectName(), juce::dontSendNotification);
        nameLabel.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
        nameLabel.setColour (juce::Label::textColourId, KngTheme::text);

        addAndMakeVisible (pathLabel);
        pathLabel.setText (session.getProjectFolder().getFullPathName(), juce::dontSendNotification);
        pathLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
        pathLabel.setColour (juce::Label::textColourId, KngTheme::muted);

        auto setup = [this] (juce::Label& l, const juce::String& text)
        {
            addAndMakeVisible (l);
            l.setText (text, juce::dontSendNotification);
            l.setColour (juce::Label::textColourId, KngTheme::text);
        };

        setup (countLabel, "Count-in");
        setup (quantLabel, "Record quantize");
        setup (snapLabel, "Snap grid");

        // Count-in
        addAndMakeVisible (countBox);
        countBox.setTooltip ("Bars of click before recording starts");
        countBox.addItem ("Off", 1);
        countBox.addItem ("1 bar", 2);
        countBox.addItem ("2 bars", 3);
        countBox.setSelectedId (juce::jlimit (0, 2, session.getCountInBars()) + 1, juce::dontSendNotification);
        countBox.onChange = [this] { session.setCountInBars (countBox.getSelectedId() - 1); };

        // Record quantize
        addAndMakeVisible (quantBox);
        quantBox.setTooltip ("When recording stops, recorded notes snap to this grid");
        const char* quantNames[] = { "Off", "1/4", "1/8", "1/16", "1/32" };

        for (int i = 0; i < 5; ++i)
            quantBox.addItem (quantNames[i], i + 1);

        {
            int id = 1;

            for (int i = 0; i < 5; ++i)
                if (std::abs (quantValues[i] - session.getRecordQuantize()) < 0.0001)
                    id = i + 1;

            quantBox.setSelectedId (id, juce::dontSendNotification);
        }

        quantBox.onChange = [this] { session.setRecordQuantize (quantValues[juce::jlimit (0, 4, quantBox.getSelectedId() - 1)]); };

        // Snap grid
        addAndMakeVisible (snapBox);
        snapBox.setTooltip ("How coarse the Snap grid is");
        const char* snapNames[] = { "Bar", "1/4 (beat)", "1/8", "1/16", "1/32" };

        for (int i = 0; i < 5; ++i)
            snapBox.addItem (snapNames[i], i + 1);

        {
            int id = 2;

            for (int i = 0; i < 5; ++i)
                if (snapValues[i] == session.getSnapGrid())
                    id = i + 1;

            snapBox.setSelectedId (id, juce::dontSendNotification);
        }

        snapBox.onChange = [this] { session.setSnapGrid (snapValues[juce::jlimit (0, 4, snapBox.getSelectedId() - 1)]); };

        // Switches
        addAndMakeVisible (clickToggle);
        clickToggle.setButtonText ("Click (metronome) on");
        clickToggle.setToggleState (session.isClickEnabled(), juce::dontSendNotification);
        clickToggle.onClick = [this] { session.setClickEnabled (clickToggle.getToggleState()); };

        addAndMakeVisible (snapToggle);
        snapToggle.setButtonText ("Snap on");
        snapToggle.setToggleState (session.isSnapEnabled(), juce::dontSendNotification);
        snapToggle.onClick = [this] { session.setSnapEnabled (snapToggle.getToggleState()); };

        // Folders and backups
        addAndMakeVisible (openFolderButton);
        openFolderButton.setTooltip ("Open this song's folder in Explorer");
        openFolderButton.onClick = [this] { session.getProjectFolder().startAsProcess(); };

        addAndMakeVisible (backupButton);
        backupButton.setTooltip ("Save a backup of the song right now");
        backupButton.onClick = [this] { session.makeBackup(); };

        addAndMakeVisible (openBackupsButton);
        openBackupsButton.setTooltip ("Open the folder where backups are kept");
        openBackupsButton.onClick = [this]
        {
            auto folder = session.getBackupsFolder();
            folder.createDirectory();
            folder.startAsProcess();
        };

        setSize (460, 380);
    }

    void paint (juce::Graphics& g) override   { g.fillAll (KngTheme::bg); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (20);
        nameLabel.setBounds (r.removeFromTop (30));
        pathLabel.setBounds (r.removeFromTop (20));
        r.removeFromTop (16);

        auto row = [&r] (juce::Label& label, juce::Component& control)
        {
            auto line = r.removeFromTop (30);
            label.setBounds (line.removeFromLeft (150));
            control.setBounds (line.removeFromLeft (170));
            r.removeFromTop (8);
        };

        row (countLabel, countBox);
        row (quantLabel, quantBox);
        row (snapLabel, snapBox);

        r.removeFromTop (6);
        clickToggle.setBounds (r.removeFromTop (28));
        snapToggle.setBounds (r.removeFromTop (28));

        r.removeFromTop (14);
        openFolderButton.setBounds (r.removeFromTop (32));
        r.removeFromTop (8);

        auto buttons = r.removeFromTop (32);
        backupButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 4));
        buttons.removeFromLeft (8);
        openBackupsButton.setBounds (buttons);
    }

    /** Opens the window (or brings it forward). Returns the new window, or nullptr if it was already open. */
    static juce::DialogWindow* open (EditSession& s)
    {
        static juce::Component::SafePointer<juce::DialogWindow> existing;

        if (existing != nullptr)
        {
            existing->setVisible (true);
            existing->toFront (true);
            return nullptr;
        }

        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (new ProjectSettings (s));
        o.dialogTitle = "Project Settings";
        o.dialogBackgroundColour = KngTheme::bg;
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = false;
        o.resizable = false;

        auto* w = o.create();
        w->setVisible (true);
        existing = w;
        return w;
    }

private:
    static constexpr double quantValues[5] = { 0.0, 1.0, 0.5, 0.25, 0.125 };
    static constexpr int snapValues[5] = { 0, 1, 2, 4, 8 };

    EditSession& session;
    juce::Label nameLabel, pathLabel, countLabel, quantLabel, snapLabel;
    juce::ComboBox countBox, quantBox, snapBox;
    juce::ToggleButton clickToggle, snapToggle;
    juce::TextButton openFolderButton { "Open Project Folder" }, backupButton { "Save Backup Now" },
                     openBackupsButton { "Open Backups Folder" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectSettings)
};
