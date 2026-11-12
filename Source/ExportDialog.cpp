#include "ExportDialog.h"

ExportDialog::ExportDialog (EditSession& sessionToUse)
    : session (sessionToUse)
{
    for (auto* c : std::initializer_list<juce::Component*> { &whatLabel, &whatBox, &formatLabel, &formatBox,
                                                             &rangeLabel, &rangeBox, &normaliseLabel, &normaliseBox,
                                                             &tracksLabel, &trackViewport, &allButton, &noneButton,
                                                             &exportButton, &openFolderButton, &progressBar, &statusLabel })
        addAndMakeVisible (c);

    whatBox.addItem ("Full mix", 1);
    whatBox.addItem ("Stems: one per track (WAV)", 2);
    whatBox.addItem ("Stems: one per folder (WAV)", 3);
    whatBox.setSelectedId (1, juce::dontSendNotification);
    whatBox.onChange = [this] { updateVisibility(); };

    formatBox.addItem ("WAV 24-bit", 1);
    formatBox.addItem ("MP3 320k", 2);
    formatBox.setSelectedId (1, juce::dontSendNotification);

    rangeBox.addItem ("Whole song", 1);
    rangeBox.addItem ("Loop region", 2);
    rangeBox.setSelectedId (1, juce::dontSendNotification);

    normaliseBox.addItem ("Off", 1);
    normaliseBox.addItem ("Peak -1 dB", 2);
    normaliseBox.addItem ("Loudness -14 LUFS", 3);
    normaliseBox.setSelectedId (1, juce::dontSendNotification);

    trackViewport.setViewedComponent (&trackList, false);
    trackViewport.setScrollBarsShown (true, false);

    allButton.onClick  = [this] { for (auto* t : trackToggles) t->setToggleState (true, juce::dontSendNotification); };
    noneButton.onClick = [this] { for (auto* t : trackToggles) t->setToggleState (false, juce::dontSendNotification); };

    exportButton.onClick = [this] { startExport(); };
    openFolderButton.onClick = [this] { session.getExportsFolder().startAsProcess(); };

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setText ("Files are named after the song and go in its Exports folder.", juce::dontSendNotification);

    rebuildTrackList();
    updateVisibility();

    startTimerHz (15);
    setSize (500, 470);
}

ExportDialog::~ExportDialog()
{
    stopTimer();
    trackViewport.setViewedComponent (nullptr, false);
    handle = nullptr;   // cancels any render still running
}

void ExportDialog::rebuildTrackList()
{
    trackToggles.clear();
    toggleTracks.clear();

    for (int i = 0; i < session.getNumTracks(); ++i)
    {
        if (! session.trackHasContent (i))
            continue;

        auto* t = session.getTrack (i);
        auto* toggle = trackToggles.add (new juce::ToggleButton (t != nullptr ? t->getName() : "Track " + juce::String (i + 1)));
        toggle->setToggleState (true, juce::dontSendNotification);
        trackList.addAndMakeVisible (toggle);
        toggleTracks.add (i);
    }

    trackList.setSize (440, juce::jmax (1, trackToggles.size() * 26));

    for (int i = 0; i < trackToggles.size(); ++i)
        trackToggles[i]->setBounds (4, i * 26, 420, 24);
}

void ExportDialog::updateVisibility()
{
    const int what = whatBox.getSelectedId();
    const bool mix = what == 1;

    formatBox.setEnabled (mix);
    normaliseBox.setEnabled (mix);

    // The checklist only applies to one-per-track stems
    for (auto* c : std::initializer_list<juce::Component*> { &tracksLabel, &trackViewport, &allButton, &noneButton })
        c->setVisible (what == 2);
}

//==============================================================================
void ExportDialog::startExport()
{
    if (exporting)
        return;

    if (session.getTransport().isRecording())
    {
        statusLabel.setText ("Stop recording first.", juce::dontSendNotification);
        return;
    }

    const bool loopOnly = rangeBox.getSelectedId() == 2;

    if (loopOnly && session.getLoopEnd() <= session.getLoopStart())
    {
        statusLabel.setText ("Set a loop region on the ruler first.", juce::dontSendNotification);
        return;
    }

    const int what = whatBox.getSelectedId();
    const bool asMp3 = what == 1 && formatBox.getSelectedId() == 2;

    if (asMp3 && ! EditSession::getLameExecutable().existsAsFile())
    {
        statusLabel.setText ("MP3 needs lame.exe in " + EditSession::getLameExecutable().getParentDirectory().getFullPathName(),
                             juce::dontSendNotification);
        return;
    }

    const auto songName = juce::File::createLegalFileName (session.getProjectName());
    juce::Component::SafePointer<ExportDialog> safe (this);

    if (what == 1)
    {
        exporting = true;
        exportingStems = false;
        exportButton.setEnabled (false);
        progress = 0.0;

        // "My Song.wav", then "My Song (2).wav" and so on
        auto dest = session.getExportsFolder().getNonexistentChildFile (songName, asMp3 ? ".mp3" : ".wav", true);
        statusLabel.setText ("Exporting " + dest.getFileName() + "...", juce::dontSendNotification);

        handle = session.exportMix (dest, loopOnly, normaliseBox.getSelectedId() - 1, asMp3,
                                    [safe, dest] (const juce::String& error)
                                    {
                                        if (safe != nullptr)
                                            safe->finish (error, dest.getParentDirectory());
                                    });
        return;
    }

    // Stems: work out what goes in each file
    stemJobs.clear();

    if (what == 2)
    {
        for (int i = 0; i < trackToggles.size(); ++i)
        {
            if (! trackToggles[i]->getToggleState())
                continue;

            const int trackIndex = toggleTracks[i];
            auto* t = session.getTrack (trackIndex);
            stemJobs.push_back ({ t != nullptr ? t->getName() : "Track", { trackIndex } });
        }
    }
    else
    {
        // One stem per folder; tracks outside folders get their own
        juce::StringArray doneFolders;

        for (int i = 0; i < session.getNumTracks(); ++i)
        {
            if (! session.trackHasContent (i))
                continue;

            const auto folder = session.getTrackFolder (i);

            if (folder.isEmpty())
            {
                auto* t = session.getTrack (i);
                stemJobs.push_back ({ t != nullptr ? t->getName() : "Track", { i } });
            }
            else if (! doneFolders.contains (folder))
            {
                doneFolders.add (folder);
                juce::Array<int> members;

                for (int m : session.getFolderTracks (folder))
                    if (session.trackHasContent (m))
                        members.add (m);

                stemJobs.push_back ({ folder, members });
            }
        }
    }

    if (stemJobs.empty())
    {
        statusLabel.setText ("Nothing selected to export.", juce::dontSendNotification);
        return;
    }

    exporting = true;
    exportingStems = true;
    stemLoopOnly = loopOnly;
    exportButton.setEnabled (false);
    progress = 0.0;

    stemFolder = session.getExportsFolder().getNonexistentChildFile (songName + " Stems", "", true);
    stemFolder.createDirectory();
    stemPosition = 0;

    exportNextStem();
}

void ExportDialog::exportNextStem()
{
    if (stemPosition >= (int) stemJobs.size())
    {
        finish ({}, stemFolder);
        return;
    }

    const auto job = stemJobs[(size_t) stemPosition];
    const auto fileName = juce::File::createLegalFileName (juce::String (stemPosition + 1).paddedLeft ('0', 2) + " " + job.name);
    auto dest = stemFolder.getChildFile (fileName + ".wav");

    statusLabel.setText ("Exporting stem " + juce::String (stemPosition + 1) + " of "
                         + juce::String ((int) stemJobs.size()) + ": " + job.name,
                         juce::dontSendNotification);

    ++stemPosition;

    juce::Component::SafePointer<ExportDialog> safe (this);

    handle = session.exportTracks (job.tracks, dest, stemLoopOnly,
                                   [safe] (const juce::String& error)
                                   {
                                       if (safe == nullptr)
                                           return;

                                       if (error.isNotEmpty())
                                           safe->finish (error, safe->stemFolder);
                                       else
                                           safe->exportNextStem();
                                   });
}

void ExportDialog::finish (const juce::String& error, const juce::File& folderToOpen)
{
    exporting = false;
    exportButton.setEnabled (true);

    if (error.isEmpty())
    {
        progress = 1.0;
        statusLabel.setText ("Done.", juce::dontSendNotification);

        if (folderToOpen.isDirectory())
            folderToOpen.startAsProcess();
    }
    else
    {
        statusLabel.setText ("Export failed: " + error, juce::dontSendNotification);
    }
}

void ExportDialog::timerCallback()
{
    if (! exporting || handle == nullptr)
        return;

    const double current = handle->getProgress();

    if (exportingStems && ! stemJobs.empty())
        progress = ((stemPosition - 1) + current) / (double) stemJobs.size();
    else
        progress = current;
}

//==============================================================================
void ExportDialog::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2b2f33));
}

void ExportDialog::resized()
{
    auto r = getLocalBounds().reduced (12);

    auto row = [&r] (int h = 26) { auto x = r.removeFromTop (h); r.removeFromTop (8); return x; };

    auto r1 = row();
    whatLabel.setBounds (r1.removeFromLeft (80));
    whatBox.setBounds (r1);

    auto r2 = row();
    formatLabel.setBounds (r2.removeFromLeft (80));
    formatBox.setBounds (r2);

    auto r3 = row();
    rangeLabel.setBounds (r3.removeFromLeft (80));
    rangeBox.setBounds (r3);

    auto r4 = row();
    normaliseLabel.setBounds (r4.removeFromLeft (80));
    normaliseBox.setBounds (r4);

    auto r5 = row (22);
    tracksLabel.setBounds (r5.removeFromLeft (80));
    noneButton.setBounds (r5.removeFromRight (60));
    r5.removeFromRight (4);
    allButton.setBounds (r5.removeFromRight (60));

    trackViewport.setBounds (row (130));

    auto buttons = row();
    exportButton.setBounds (buttons.removeFromLeft (120));
    buttons.removeFromLeft (8);
    openFolderButton.setBounds (buttons.removeFromLeft (170));

    progressBar.setBounds (row());
    statusLabel.setBounds (r.removeFromTop (24));
}
