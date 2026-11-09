#include "StartScreen.h"

//==============================================================================
StartScreen::Card::Card (const juce::String& t, const juce::String& s, juce::Colour a)
    : title (t), subtitle (s), accent (a)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void StartScreen::Card::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (hover ? juce::Colour (0xff2c3137) : juce::Colour (0xff24282c));
    g.fillRoundedRectangle (r, 10.0f);

    g.setColour (hover ? accent : accent.withAlpha (0.5f));
    g.drawRoundedRectangle (r, 10.0f, hover ? 2.0f : 1.0f);

    g.setColour (accent);
    g.fillRoundedRectangle (r.removeFromTop (5.0f).reduced (14.0f, 0.0f), 2.0f);

    auto text = getLocalBounds().reduced (22, 20);

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    g.drawText (title, text.removeFromTop (34), juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions (14.0f)));
    g.drawText (subtitle, text.removeFromTop (22), juce::Justification::centredLeft);
}

//==============================================================================
StartScreen::StartScreen()
{
    setWantsKeyboardFocus (true);

    addAndMakeVisible (newBeatCard);
    addAndMakeVisible (newVocalCard);
    addAndMakeVisible (continueButton);
    addAndMakeVisible (openOtherButton);
    addAndMakeVisible (recentLabel);

    newBeatCard.onClick  = [this] { if (onNewBeat) onNewBeat(); };
    newVocalCard.onClick = [this] { if (onNewVocal) onNewVocal(); };
    continueButton.onClick  = [this] { if (onContinue) onContinue(); };
    openOtherButton.onClick = [this] { if (onOpenOther) onOpenOther(); };

    recentLabel.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    recentLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
}

StartScreen::~StartScreen() = default;

juce::File StartScreen::getProjectsRoot()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("KNG Studio");
}

juce::Array<juce::File> StartScreen::findRecentProjects()
{
    juce::Array<juce::File> edits;

    for (auto& folder : getProjectsRoot().findChildFiles (juce::File::findDirectories, false))
        for (auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.tracktionedit"))
            edits.add (f);

    // Newest first
    std::sort (edits.begin(), edits.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getLastModificationTime() > b.getLastModificationTime();
    });

    edits.removeRange (8, edits.size());
    return edits;
}

void StartScreen::refresh (const juce::File& current)
{
    currentProject = current;

    continueButton.setVisible (currentProject.existsAsFile());
    continueButton.setButtonText ("Continue: " + currentProject.getFileNameWithoutExtension());

    recentButtons.clear();

    for (auto& f : findRecentProjects())
    {
        const auto when = f.getLastModificationTime().toString (true, true, false, true);
        auto* b = recentButtons.add (new juce::TextButton (f.getFileNameWithoutExtension() + "      " + when));
        b->setTooltip (f.getFullPathName());
        b->onClick = [this, f] { if (onOpenProject) onOpenProject (f); };
        addAndMakeVisible (b);
    }

    recentLabel.setVisible (! recentButtons.isEmpty());
    resized();
    repaint();
}

//==============================================================================
void StartScreen::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff17191b));

    auto r = getLocalBounds().withSizeKeepingCentre (juce::jmin (820, getWidth() - 40), getHeight());

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (40.0f, juce::Font::bold)));
    g.drawText ("KNG Studio", r.getX(), 50, r.getWidth(), 50, juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.setFont (juce::Font (juce::FontOptions (15.0f)));
    g.drawText ("What are we making today?", r.getX(), 100, r.getWidth(), 24, juce::Justification::centredLeft);

    if (currentProject.existsAsFile())
    {
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText ("Esc to go back to your project", r.getX(), getHeight() - 34, r.getWidth(), 20,
                    juce::Justification::centredLeft);
    }
}

void StartScreen::resized()
{
    auto r = getLocalBounds().withSizeKeepingCentre (juce::jmin (820, getWidth() - 40), getHeight());
    r.removeFromTop (150);

    auto cards = r.removeFromTop (130);
    newBeatCard.setBounds (cards.removeFromLeft (cards.getWidth() / 2 - 8));
    cards.removeFromLeft (16);
    newVocalCard.setBounds (cards);

    r.removeFromTop (20);
    auto buttons = r.removeFromTop (34);

    if (continueButton.isVisible())
    {
        continueButton.setBounds (buttons.removeFromLeft (300));
        buttons.removeFromLeft (10);
    }

    openOtherButton.setBounds (buttons.removeFromLeft (150));

    r.removeFromTop (26);
    recentLabel.setBounds (r.removeFromTop (22));

    for (auto* b : recentButtons)
    {
        b->setBounds (r.removeFromTop (30));
        r.removeFromTop (4);
    }
}

bool StartScreen::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && currentProject.existsAsFile())
    {
        if (onContinue)
            onContinue();

        return true;
    }

    return false;
}
