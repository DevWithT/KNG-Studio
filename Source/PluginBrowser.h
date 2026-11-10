#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "EditSession.h"

/** Every instrument and effect in one searchable list.
    Drag one onto the tracks or the Rack, or double-click it. */
class PluginBrowser : public juce::Component,
                      private juce::ListBoxModel,
                      private juce::Timer
{
public:
    struct Entry
    {
        juce::String name, detail, id;
        bool instrument = false;
        bool header = false;
    };

    explicit PluginBrowser (tracktion::Engine& engineToUse);
    ~PluginBrowser() override;

    void setSession (EditSession* sessionToUse)   { session = sessionToUse; }

    void paint (juce::Graphics&) override;
    void resized() override;

    static std::vector<Entry> buildEntries (tracktion::Engine& engine);

    /** A quick menu of every instrument, for the "+ Instrument" buttons. */
    static void showInstrumentMenu (tracktion::Engine& engine, juce::Component& anchor,
                                    std::function<void (const Entry&)> onPick);

    /** Drags look like "kngplugin|instrument|<id>|<name>". Returns false for anything else. */
    static bool parseDrag (const juce::var& description, Entry& out);

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    juce::String getTooltipForRow (int row) override;

    void timerCallback() override;
    void refresh();

    tracktion::Engine& engine;
    EditSession* session = nullptr;

    juce::TextEditor search;
    juce::ListBox list;
    std::vector<Entry> all, shown;
    int lastCount = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginBrowser)
};
