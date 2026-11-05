#include "ExportDialog.h"

ExportDialog::ExportDialog (EditSession& sessionToUse)
    : session (sessionToUse)
{
    for (auto* c : std::initializer_list<juce::Component*> { &whatLabel, &whatBox, &formatLabel, &formatBox,
                                                             &rangeLabel, &rangeBox, &normaliseLabel, &normaliseBox,
                                                             &exportButton, &progressBar, &statusLabel })
        addAndMakeVisible (c);

    whatBox.addItem ("Full mix", 1);
    whatBox.addItem ("Stems, one per track (always WAV)", 2);
    whatBox.setSelectedId (1, juce::dontSendNotification);
    whatBox.onChange = [this]
    {
        const bool mix = whatBox.getSelectedId() == 1;
        normaliseBox.setEnabled (mix);
        formatBox.setEnabled (mix);
    };

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

    exportButton.onClick = [this] { startExport(); };

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setText ("Files go to this project's Exports folder.", juce::dontSendNotification);

    startTimerHz (15);
    setSize (460, 280);
}

ExportDialog::~ExportDialog()
{
    stopTimer();
    handle = nullptr;   // cancels any render still running
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

    const bool isMix = whatBox.getSelectedId() == 1;
    const bool asMp3 = isMix && formatBox.getSelectedId() == 2;

    if (asMp3 && ! EditSession::getLameExecutable().existsAsFile())
    {
        statusLabel.setText ("MP3 needs lame.exe in " + EditSession::getLameExecutable().getParentDirectory().getFullPathName(),
                             juce::dontSendNotification);
        return;
    }

    exporting = true;
    exportButton.setEnabled (false);
    progress = 0.0;

    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S");
    juce::Component::SafePointer<ExportDialog> safe (this);

    if (isMix)
    {
        exportingStems = false;

        auto dest = session.getExportsFolder().getChildFile (session.getProjectName() + " " + stamp
                                                             + (asMp3 ? ".mp3" : ".wav"));
        statusLabel.setText ("Exporting mix...", juce::dontSendNotification);

        handle = session.exportMix (dest, loopOnly, normaliseBox.getSelectedId() - 1, asMp3,
                                    [safe, dest] (const juce::String& error)
                                    {
                                        if (safe != nullptr)
                                            safe->finish (error, dest.getParentDirectory());
                                    });
    }
    else
    {
        exportingStems = true;
        stemLoopOnly = loopOnly;

        stemFolder = session.getExportsFolder().getChildFile (session.getProjectName() + " Stems " + stamp);
        stemFolder.createDirectory();

        // Skip empty tracks
        stemTracks.clear();

        for (int i = 0; i < session.getNumTracks(); ++i)
            if (session.trackHasContent (i))
                stemTracks.add (i);

        stemPosition = 0;

        if (stemTracks.isEmpty())
        {
            finish ("No tracks have anything on them.", {});
            return;
        }

        exportNextStem();
    }
}

void ExportDialog::exportNextStem()
{
    if (stemPosition >= stemTracks.size())
    {
        finish ({}, stemFolder);
        return;
    }

    const int trackIndex = stemTracks[stemPosition];
    auto* track = session.getTrack (trackIndex);

    const auto trackName = track != nullptr ? track->getName() : juce::String ("Track");
    const auto fileName = juce::File::createLegalFileName (juce::String (trackIndex + 1).paddedLeft ('0', 2)
                                                           + " " + trackName);

    auto dest = stemFolder.getChildFile (fileName + ".wav");

    statusLabel.setText ("Exporting stem " + juce::String (stemPosition + 1) + " of "
                         + juce::String (stemTracks.size()) + ": " + trackName,
                         juce::dontSendNotification);

    ++stemPosition;

    juce::Component::SafePointer<ExportDialog> safe (this);

    handle = session.exportStem (trackIndex, dest, stemLoopOnly,
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

    if (exportingStems && stemTracks.size() > 0)
        progress = ((stemPosition - 1) + current) / (double) stemTracks.size();
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

    auto row = [&r] { auto x = r.removeFromTop (26); r.removeFromTop (8); return x; };

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

    exportButton.setBounds (row().removeFromLeft (120));
    progressBar.setBounds (row());
    statusLabel.setBounds (r.removeFromTop (24));
}
