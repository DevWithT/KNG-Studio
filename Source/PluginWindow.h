#pragma once

#include <tracktion_engine/tracktion_engine.h>

//==============================================================================
/** A window that shows a plugin's editor. Tracktion asks the app to make these. */
class PluginWindow : public juce::DocumentWindow
{
public:
    explicit PluginWindow (tracktion::Plugin&);
    ~PluginWindow() override;

    static std::unique_ptr<juce::Component> create (tracktion::Plugin&);

    void show();
    void setEditor (std::unique_ptr<tracktion::Plugin::EditorComponent>);
    tracktion::Plugin::EditorComponent* getEditor() const    { return editor.get(); }

    void recreateEditor();
    void recreateEditorAsync();

private:
    void moved() override;
    void userTriedToCloseWindow() override          { plugin.windowState->closeWindowExplicitly(); }
    void closeButtonPressed() override              { userTriedToCloseWindow(); }
    float getDesktopScaleFactor() const override    { return 1.0f; }

    std::unique_ptr<tracktion::Plugin::EditorComponent> editor;
    tracktion::Plugin& plugin;
    bool updateStoredBounds = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};

//==============================================================================
/** Hooks our plugin windows into the engine. */
class KngUIBehaviour : public tracktion::UIBehaviour
{
public:
    KngUIBehaviour() = default;

    std::unique_ptr<juce::Component> createPluginWindow (tracktion::PluginWindowState&) override;
    void recreatePluginWindowContentAsync (tracktion::Plugin&) override;
};
