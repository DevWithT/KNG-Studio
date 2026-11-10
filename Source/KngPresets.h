#pragma once

#include <tracktion_engine/tracktion_engine.h>

/** Your own presets for any KNG plugin, saved in Documents\KNG Studio\Presets\<plugin>. */
namespace KngPresets
{
    juce::File folderFor (tracktion::Plugin& plugin);
    juce::Array<juce::File> list (tracktion::Plugin& plugin);
    bool save (tracktion::Plugin& plugin, const juce::String& name);
    bool load (tracktion::Plugin& plugin, const juce::File& file);
}

/** Preset drop-down: factory presets, your presets, Save Preset..., Open Presets Folder. */
class KngPresetMenu : public juce::ComboBox
{
public:
    KngPresetMenu();

    void attach (tracktion::Plugin& pluginToUse, const juce::StringArray& factoryNames,
                 std::function<void (int)> applyFactory);

    void rebuild();

private:
    void picked();
    void beginEdit();

    tracktion::Plugin* plugin = nullptr;
    juce::StringArray factory;
    std::function<void (int)> onFactory;
    juce::Array<juce::File> userFiles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngPresetMenu)
};
