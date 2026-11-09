#pragma once

#include <tracktion_engine/tracktion_engine.h>

/** One place for audio, plugins, recording, folders and about. */
class SettingsComponent : public juce::Component
{
public:
    explicit SettingsComponent (tracktion::Engine& engineToUse);
    ~SettingsComponent() override;

    std::function<void()> onLatencyCheck;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Page : public juce::Component
    {
        void resized() override;
        juce::OwnedArray<juce::Component> items;
        juce::Array<int> heights;
    };

    static juce::File getRoot();
    juce::Component* makeRecordingPage();
    juce::Component* makeFoldersPage();
    juce::Component* makeAboutPage();

    tracktion::Engine& engine;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsComponent)
};
