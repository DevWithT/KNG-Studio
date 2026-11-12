#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>

//==============================================================================
/** Plays a sample once when you click it in the browser, optionally sped up or slowed to the song's tempo. */
class SamplePreviewer
{
public:
    explicit SamplePreviewer (juce::AudioDeviceManager& deviceManagerToUse);
    ~SamplePreviewer();

    void play (const juce::File& file, double speedRatio = 1.0);

private:
    juce::AudioDeviceManager& deviceManager;
    juce::AudioFormatManager formats;
    juce::AudioTransportSource transport;
    juce::ResamplingAudioSource resampler { &transport, false, 2 };
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioSourcePlayer player;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplePreviewer)
};

//==============================================================================
/** Left-side Sounds panel: the Kits folder, search across everything, and favorites. */
class KitBrowser : public juce::Component,
                   private juce::FileBrowserListener,
                   private juce::ListBoxModel,
                   private juce::Timer
{
public:
    explicit KitBrowser (juce::AudioDeviceManager& deviceManager);
    ~KitBrowser() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Set by the main window so loops can preview at the song's tempo. */
    std::function<double()> getProjectBpm;

    /** Search results and favorites drag as "kngfile|<path>". */
    static bool getDraggedFile (const juce::var& description, juce::File& out);

    /** "drill loop 140bpm.wav" -> 140. Returns 0 if the name has no tempo. */
    static double bpmFromName (const juce::String& name);

private:
    void timerCallback() override;
    juce::String getFolderSignature() const;

    // Folder tree
    void selectionChanged() override;
    void fileClicked (const juce::File&, const juce::MouseEvent&) override;
    void fileDoubleClicked (const juce::File&) override {}
    void browserRootChanged (const juce::File&) override {}

    // Search results and favorites list
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override;
    juce::String getTooltipForRow (int row) override;

    void preview (const juce::File& file);
    void updateList();
    void showFileMenu (const juce::File& file);
    void loadFavorites();
    void saveFavorites();
    bool isFavorite (const juce::File& file) const;
    void toggleFavorite (const juce::File& file);

    juce::File kitsFolder;
    juce::TimeSliceThread scanThread { "Kit scanner" };
    juce::WildcardFileFilter filter { "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg", "*", "Audio files" };
    juce::DirectoryContentsList contents { &filter, scanThread };
    juce::FileTreeComponent tree { contents };

    juce::Label title { {}, "Sounds" };
    juce::TextButton openFolderButton { "Open Folder" };
    juce::ComboBox viewBox;
    juce::ToggleButton syncButton { "Sync" };
    juce::TextEditor search;
    juce::ListBox list;

    SamplePreviewer previewer;
    juce::String lastSignature;
    juce::Array<juce::File> allFiles, shown, favorites;
    bool allFilesDirty = true;
    int searchCountdown = 0, signatureCountdown = 20;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KitBrowser)
};
