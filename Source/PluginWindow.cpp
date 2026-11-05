#include "PluginWindow.h"

namespace te = tracktion;

//==============================================================================
/** Wraps a VST3's own editor (or a generic one if it has none). */
struct AudioProcessorEditorContentComp : public te::Plugin::EditorComponent
{
    explicit AudioProcessorEditorContentComp (te::ExternalPlugin& plug) : plugin (plug)
    {
        JUCE_AUTORELEASEPOOL
        {
            if (auto pi = plugin.getAudioPluginInstance())
            {
                editor.reset (pi->createEditorAndMakeActive());

                if (editor == nullptr)
                    editor = std::make_unique<juce::GenericAudioProcessorEditor> (*pi);

                addAndMakeVisible (*editor);
            }
        }

        resizeToFitEditor (true);
    }

    bool allowWindowResizing() override { return false; }

    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override
    {
        if (editor == nullptr || allowWindowResizing())
            return {};

        return editor->getConstrainer();
    }

    void resized() override
    {
        if (editor != nullptr)
            editor->setBounds (getLocalBounds());
    }

    void childBoundsChanged (juce::Component* c) override
    {
        if (c == editor.get())
        {
            plugin.edit.pluginChanged (plugin);
            resizeToFitEditor();
        }
    }

    void resizeToFitEditor (bool force = false)
    {
        if (force || ! allowWindowResizing())
            setSize (juce::jmax (8, editor != nullptr ? editor->getWidth() : 0),
                     juce::jmax (8, editor != nullptr ? editor->getHeight() : 0));
    }

    te::ExternalPlugin& plugin;
    std::unique_ptr<juce::AudioProcessorEditor> editor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioProcessorEditorContentComp)
};

static std::unique_ptr<te::Plugin::EditorComponent> createEditorFor (te::Plugin& plugin)
{
    if (auto* ext = dynamic_cast<te::ExternalPlugin*> (&plugin))
        return std::make_unique<AudioProcessorEditorContentComp> (*ext);

    return plugin.createEditor();
}

//==============================================================================
PluginWindow::PluginWindow (te::Plugin& plug)
    : DocumentWindow (plug.getName(), juce::Colours::black, DocumentWindow::closeButton, true),
      plugin (plug)
{
    getConstrainer()->setMinimumOnscreenAmounts (0x10000, 50, 30, 50);
    setResizeLimits (100, 50, 4000, 4000);

    recreateEditor();

    setBoundsConstrained (getLocalBounds() + plugin.windowState->choosePositionForPluginWindow());

    updateStoredBounds = true;
}

PluginWindow::~PluginWindow()
{
    updateStoredBounds = false;
    plugin.edit.flushPluginStateIfNeeded (plugin);
    setEditor (nullptr);
}

void PluginWindow::show()
{
    setVisible (true);
    toFront (false);
    setBoundsConstrained (getBounds());
}

void PluginWindow::setEditor (std::unique_ptr<te::Plugin::EditorComponent> newEditor)
{
    JUCE_AUTORELEASEPOOL
    {
        setConstrainer (nullptr);
        editor.reset();

        if (newEditor != nullptr)
        {
            editor = std::move (newEditor);
            setContentNonOwned (editor.get(), true);
        }

        setResizable (editor == nullptr || editor->allowWindowResizing(), false);

        if (editor != nullptr && editor->allowWindowResizing())
            setConstrainer (editor->getBoundsConstrainer());
    }
}

std::unique_ptr<juce::Component> PluginWindow::create (te::Plugin& plugin)
{
    if (auto* externalPlugin = dynamic_cast<te::ExternalPlugin*> (&plugin))
        if (externalPlugin->getAudioPluginInstance() == nullptr)
            return nullptr;

    std::unique_ptr<PluginWindow> w;

    {
        // Blocks clicks elsewhere while the plugin builds its editor
        struct Blocker : public juce::Component { void inputAttemptWhenModal() override {} };

        Blocker blocker;
        blocker.enterModalState (false);

        w = std::make_unique<PluginWindow> (plugin);
    }

    if (w == nullptr || w->getEditor() == nullptr)
        return {};

    w->show();
    return w;
}

void PluginWindow::recreateEditor()
{
    setEditor (nullptr);
    setEditor (createEditorFor (plugin));
}

void PluginWindow::recreateEditorAsync()
{
    setEditor (nullptr);

    juce::Timer::callAfterDelay (50, [this, sp = juce::Component::SafePointer<juce::Component> (this)]
    {
        if (sp != nullptr)
            recreateEditor();
    });
}

void PluginWindow::moved()
{
    if (updateStoredBounds)
    {
        plugin.windowState->lastWindowBounds = getBounds();
        plugin.edit.pluginChanged (plugin);
    }
}

//==============================================================================
std::unique_ptr<juce::Component> KngUIBehaviour::createPluginWindow (te::PluginWindowState& pws)
{
    if (auto* ws = dynamic_cast<te::Plugin::WindowState*> (&pws))
        return PluginWindow::create (ws->plugin);

    return {};
}

void KngUIBehaviour::recreatePluginWindowContentAsync (te::Plugin& p)
{
    if (auto* w = dynamic_cast<PluginWindow*> (p.windowState->pluginWindow.get()))
        return w->recreateEditorAsync();

    UIBehaviour::recreatePluginWindowContentAsync (p);
}
