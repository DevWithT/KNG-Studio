#include "MainComponent.h"
#include "PluginWindow.h"

class KngStudioApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "KNG Studio"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String&) override
    {
        engine = std::make_unique<tracktion::Engine> (getApplicationName(), std::make_unique<KngUIBehaviour>(), nullptr);
        mainWindow = std::make_unique<MainWindow> (getApplicationName(), *engine);
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        engine = nullptr;
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
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<tracktion::Engine> engine;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (KngStudioApplication)
