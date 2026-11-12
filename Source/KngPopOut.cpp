#include "KngPopOut.h"

namespace te = tracktion;

namespace
{
    std::function<void (te::Plugin&)>& dockHandler()
    {
        static std::function<void (te::Plugin&)> handler;
        return handler;
    }

    juce::Array<juce::Component::SafePointer<juce::Component>>& openWindows()
    {
        static juce::Array<juce::Component::SafePointer<juce::Component>> windows;
        return windows;
    }

    /** Dock and On Top, above the plugin's panel. */
    class TopBar : public juce::Component
    {
    public:
        TopBar()
        {
            addAndMakeVisible (dockButton);
            addAndMakeVisible (topButton);
            topButton.setClickingTogglesState (true);
            topButton.setToggleState (true, juce::dontSendNotification);
            dockButton.setTooltip ("Put this panel back in the bottom panel");
            topButton.setTooltip ("Keep this window above the main window");
        }

        void paint (juce::Graphics& g) override   { g.fillAll (juce::Colour (0xff111213)); }

        void resized() override
        {
            auto r = getLocalBounds().reduced (6, 4);
            topButton.setBounds (r.removeFromRight (80));
            r.removeFromRight (6);
            dockButton.setBounds (r.removeFromRight (70));
        }

        juce::TextButton dockButton { "Dock" }, topButton { "On Top" };
    };

    class Content : public juce::Component
    {
    public:
        explicit Content (std::unique_ptr<juce::Component> editorToShow)
            : editor (std::move (editorToShow))
        {
            addAndMakeVisible (bar);

            if (editor != nullptr)
                addAndMakeVisible (*editor);

            const int w = editor != nullptr ? juce::jmax (400, editor->getWidth()) : 600;
            const int h = editor != nullptr ? juce::jmax (200, editor->getHeight()) : 400;
            setSize (w, h + barHeight);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            bar.setBounds (r.removeFromTop (barHeight));

            if (editor != nullptr)
                editor->setBounds (r);
        }

        static constexpr int barHeight = 32;
        TopBar bar;
        std::unique_ptr<juce::Component> editor;
    };

    class Window : public juce::DocumentWindow,
                   private juce::Timer
    {
    public:
        explicit Window (te::Plugin& p)
            : juce::DocumentWindow (titleFor (p), juce::Colour (0xff111213),
                                    juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
              plugin (&p)
        {
            setUsingNativeTitleBar (true);

            auto* content = new Content (p.createEditor());
            content->bar.dockButton.onClick = [this] { dockAndClose(); };
            content->bar.topButton.onClick = [this, content] { setAlwaysOnTop (content->bar.topButton.getToggleState()); };
            setContentOwned (content, true);

            setResizable (true, false);
            setResizeLimits (400, 240, 3000, 2000);
            centreWithSize (getWidth(), getHeight());
            setAlwaysOnTop (true);
            setVisible (true);
            startTimerHz (2);
        }

        ~Window() override
        {
            stopTimer();
        }

        void closeButtonPressed() override   { closeSoon(); }
        te::Plugin* getPlugin() const        { return plugin.get(); }

    private:
        static juce::String titleFor (te::Plugin& p)
        {
            auto title = p.getName();

            if (auto* t = p.getOwnerTrack())
                title << " - " << t->getName();

            return title;
        }

        // The plugin was deleted: its window goes too
        void timerCallback() override
        {
            if (plugin == nullptr || plugin->getOwnerList() == nullptr)
                closeSoon();
        }

        void dockAndClose()
        {
            if (plugin != nullptr && dockHandler())
                dockHandler() (*plugin);

            closeSoon();
        }

        void closeSoon()
        {
            stopTimer();
            juce::Component::SafePointer<Window> safe (this);
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) delete safe.getComponent(); });
        }

        te::Plugin::Ptr plugin;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Window)
    };
}

namespace KngPopOut
{
    void open (te::Plugin& plugin)
    {
        auto& windows = openWindows();

        for (int i = windows.size(); --i >= 0;)
            if (windows[i] == nullptr)
                windows.remove (i);

        for (auto& w : windows)
        {
            if (auto* win = dynamic_cast<Window*> (w.getComponent()))
            {
                if (win->getPlugin() == &plugin)
                {
                    win->toFront (true);
                    return;
                }
            }
        }

        windows.add (new Window (plugin));
    }

    bool isPoppedOut (const juce::Component& component)
    {
        return dynamic_cast<Window*> (component.getTopLevelComponent()) != nullptr;
    }

    void closeAll()
    {
        auto& windows = openWindows();

        for (auto& w : windows)
            if (w != nullptr)
                delete w.getComponent();

        windows.clear();
    }

    void setDockHandler (std::function<void (te::Plugin&)> handler)
    {
        dockHandler() = std::move (handler);
    }
}
