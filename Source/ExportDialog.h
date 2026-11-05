#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

class ExportDialog : public juce::Component,
                     private juce::Timer
{
public:
    explicit ExportDialog (EditSession& sessionToUse);
    ~ExportDialog() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void startExport();
    void exportNextStem();
    void finish (const juce::String& error, const juce::File& folderToOpen);

    EditSession& session;

    juce::Label whatLabel { {}, "Export" }, formatLabel { {}, "Format" },
                rangeLabel { {}, "Range" }, normaliseLabel { {}, "Normalize" };
    juce::ComboBox whatBox, formatBox, rangeBox, normaliseBox;
    juce::TextButton exportButton { "Export" };
    juce::Label statusLabel;

    double progress = 0.0;
    juce::ProgressBar progressBar { progress };

    std::shared_ptr<tracktion::EditRenderer::Handle> handle;

    bool exporting = false;
    bool exportingStems = false;
    bool stemLoopOnly = false;
    juce::File stemFolder;
    juce::Array<int> stemTracks;
    int stemPosition = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExportDialog)
};
