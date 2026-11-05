#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

class FxChainComponent : public juce::Component,
                         private juce::Timer
{
public:
    FxChainComponent (EditSession& sessionToUse, tracktion::PluginList& listToUse);
    ~FxChainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Row : public juce::Component
    {
        Row (FxChainComponent& owner, tracktion::Plugin& p);
        void resized() override;

        tracktion::Plugin::Ptr plugin;
        juce::Label name;
        juce::TextButton openButton { "Open" }, upButton { "Up" }, downButton { "Down" }, removeButton { "X" };
        juce::ToggleButton enabledButton { "On" };
    };

    void timerCallback() override;
    void rebuild();
    void showAddMenu();

    EditSession& session;
    tracktion::PluginList& list;
    tracktion::Track::Ptr ownerTrack;   // keeps a track's plugin list alive; null for master

    juce::TextButton addButton { "+ Add Plugin" };
    juce::OwnedArray<Row> rows;
    juce::Array<tracktion::Plugin*> shown;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxChainComponent)
};

//==============================================================================
class FxChainWindow : public juce::DocumentWindow
{
public:
    FxChainWindow (const juce::String& title, EditSession& session, tracktion::PluginList& list);

    void closeButtonPressed() override              { setVisible (false); }
    tracktion::PluginList* getList() const          { return list; }

private:
    tracktion::PluginList* list;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxChainWindow)
};
