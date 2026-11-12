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

    /** Tells the engine which song is open, so MIDI controller moves and MIDI learn reach it. */
    static void setCurrentEdit (tracktion::Edit* edit)        { current() = edit; }

    tracktion::Edit* getCurrentlyFocusedEdit() override       { return current(); }
    tracktion::Edit* getLastFocusedEdit() override            { return current(); }

private:
    static tracktion::Edit*& current()
    {
        static tracktion::Edit* edit = nullptr;
        return edit;
    }
};
