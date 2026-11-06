#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>

//==============================================================================
/** Plays a sample once when you click it in the browser. */
class SamplePreviewer
{
public:
    explicit SamplePreviewer (juce::AudioDeviceManager& deviceManagerToUse);
    ~SamplePreviewer();

    void play (const juce::File& file);

private:
    juce::AudioDeviceManager& deviceManager;
    juce::AudioFormatManager formats;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioSourcePlayer player;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplePreviewer)
};

//==============================================================================
/** Left-side panel showing everything in the Kits folder. Picks up new folders on its own. */
class KitBrowser : public juce::Component,
                   private juce::FileBrowserListener,
                   private juce::Timer
{
public:
    explicit KitBrowser (juce::AudioDeviceManager& deviceManager);
    ~KitBrowser() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::String getFolderSignature() const;

    void selectionChanged() override;
    void fileClicked (const juce::File&, const juce::MouseEvent&) override {}
    void fileDoubleClicked (const juce::File&) override {}
    void browserRootChanged (const juce::File&) override {}

    juce::File kitsFolder;
    juce::TimeSliceThread scanThread { "Kit scanner" };
    juce::WildcardFileFilter filter { "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg", "*", "Audio files" };
    juce::DirectoryContentsList contents { &filter, scanThread };
    juce::FileTreeComponent tree { contents };

    juce::Label title { {}, "Kits" };
    juce::TextButton openFolderButton { "Open Folder" };
    SamplePreviewer previewer;
    juce::String lastSignature;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KitBrowser)
};
