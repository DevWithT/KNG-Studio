#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Shown when KNG Studio opens: start something new or pick up where you left off. */
class StartScreen : public juce::Component
{
public:
    StartScreen();
    ~StartScreen() override;

    /** Rescan recent projects. Call this each time the screen is shown. */
    void refresh (const juce::File& currentProject);

    std::function<void()> onNewBeat;
    std::function<void()> onNewVocal;
    std::function<void()> onOpenOther;
    std::function<void()> onContinue;
    std::function<void (const juce::File&)> onOpenProject;   // a .tracktionedit file

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    struct Card : public juce::Component
    {
        Card (const juce::String& title, const juce::String& subtitle, juce::Colour accent);
        void paint (juce::Graphics&) override;
        void mouseEnter (const juce::MouseEvent&) override   { hover = true;  repaint(); }
        void mouseExit (const juce::MouseEvent&) override    { hover = false; repaint(); }
        void mouseUp (const juce::MouseEvent& e) override    { if (contains (e.getPosition()) && onClick) onClick(); }

        juce::String title, subtitle;
        juce::Colour accent;
        bool hover = false;
        std::function<void()> onClick;
    };

    static juce::File getProjectsRoot();
    juce::Array<juce::File> findRecentProjects();

    Card newBeatCard { "New Beat", "Make drums and melodies in the Rack", juce::Colour (0xff3a8ee6) };
    Card newVocalCard { "New Vocal Session", "Record and comp vocals on the timeline", juce::Colour (0xffe6a23a) };

    juce::TextButton continueButton, openOtherButton { "Open Other..." };
    juce::Label recentLabel { {}, "Recent projects" };
    juce::OwnedArray<juce::TextButton> recentButtons;
    juce::File currentProject;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StartScreen)
};
