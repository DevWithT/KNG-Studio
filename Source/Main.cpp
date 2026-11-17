#include "MainComponent.h"
#include "PluginWindow.h"
#include "KngLookAndFeel.h"
#include "KngTheme.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <dwmapi.h>
 #pragma comment (lib, "dwmapi.lib")
#endif

// Any crash writes a stack trace to Documents\KNG Studio\crash_log.txt
static void kngCrashHandler (void*)
{
    auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("KNG Studio");
    folder.createDirectory();

    folder.getChildFile ("crash_log.txt")
          .appendText ("\n==== " + juce::Time::getCurrentTime().toString (true, true) + " ====\n"
                       + juce::SystemStats::getStackBacktrace());
}

#if JUCE_WINDOWS
// Windows 11 lets an app set its title bar colors (older Windows ignores this and keeps the default)
static void kngDarkTitleBar (juce::Component& window)
{
    if (auto* peer = window.getPeer())
    {
        auto hwnd = (HWND) peer->getNativeHandle();
        const BOOL dark = TRUE;
        const COLORREF caption = RGB (KngTheme::menu.getRed(), KngTheme::menu.getGreen(), KngTheme::menu.getBlue());
        const COLORREF text    = RGB (KngTheme::text.getRed(), KngTheme::text.getGreen(), KngTheme::text.getBlue());
        const COLORREF border  = RGB (KngTheme::line.getRed(), KngTheme::line.getGreen(), KngTheme::line.getBlue());

        DwmSetWindowAttribute (hwnd, 20, &dark, sizeof (dark));         // dark mode caption buttons
        DwmSetWindowAttribute (hwnd, 34, &border, sizeof (border));     // window border
        DwmSetWindowAttribute (hwnd, 35, &caption, sizeof (caption));   // title bar
        DwmSetWindowAttribute (hwnd, 36, &text, sizeof (text));         // title text
    }
}
#endif

class KngStudioApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "KNG Studio"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String&) override
    {
        juce::SystemStats::setApplicationCrashHandler (kngCrashHandler);

        lookAndFeel = std::make_unique<KngLookAndFeel>();
        juce::LookAndFeel::setDefaultLookAndFeel (lookAndFeel.get());
        engine = std::make_unique<tracktion::Engine> (getApplicationName(), std::make_unique<KngUIBehaviour>(), nullptr);
        mainWindow = std::make_unique<MainWindow> (getApplicationName(), *engine);
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        engine = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        lookAndFeel = nullptr;
    }

    void systemRequestedQuit() override { quit(); }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, tracktion::Engine& engine)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                  .findColour (juce::ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent (engine), true);
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());

            // Put the window back where it was last time
            const auto saved = getStateFile().loadFileAsString();

            if (saved.isNotEmpty())
                restoreWindowStateFromString (saved);

            setVisible (true);

           #if JUCE_WINDOWS
            kngDarkTitleBar (*this);
           #endif
        }

        void closeButtonPressed() override
        {
            getStateFile().replaceWithText (getWindowStateAsString());
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        static juce::File getStateFile()
        {
            auto root = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("KNG Studio");
            root.createDirectory();
            return root.getChildFile ("window.txt");
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<tracktion::Engine> engine;
    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<KngLookAndFeel> lookAndFeel;
};

START_JUCE_APPLICATION (KngStudioApplication)
