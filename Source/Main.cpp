#include "MainComponent.h"
#include "PluginWindow.h"
#include "KngLookAndFeel.h"
#include "KngUIBehaviour.h"

// Any crash writes a stack trace to Documents\KNG Studio\crash_log.txt
static void kngCrashHandler (void*)
{
    auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("KNG Studio");
    folder.createDirectory();

    folder.getChildFile ("crash_log.txt")
          .appendText ("\n==== " + juce::Time::getCurrentTime().toString (true, true) + " ====\n"
                       + juce::SystemStats::getStackBacktrace());
}

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
