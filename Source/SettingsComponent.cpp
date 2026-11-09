#include "SettingsComponent.h"

namespace te = tracktion;

//==============================================================================
void SettingsComponent::Page::resized()
{
    auto r = getLocalBounds().reduced (16);

    for (int i = 0; i < items.size(); ++i)
    {
        items[i]->setBounds (r.removeFromTop (heights[i]));
        r.removeFromTop (8);
    }
}

static juce::Label* makeText (const juce::String& text, float size, float alpha)
{
    auto* l = new juce::Label ({}, text);
    l->setFont (juce::Font (juce::FontOptions (size)));
    l->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (alpha));
    l->setJustificationType (juce::Justification::topLeft);
    return l;
}

//==============================================================================
SettingsComponent::SettingsComponent (te::Engine& engineToUse)
    : engine (engineToUse)
{
    const juce::Colour tabColour (0xff24282c);

    auto* audio = new juce::AudioDeviceSelectorComponent (engine.getDeviceManager().deviceManager,
                                                          0, 256, 0, 256, true, false, false, false);

    auto& pm = engine.getPluginManager();
    auto* plugins = new juce::PluginListComponent (pm.pluginFormatManager, pm.knownPluginList,
                                                   getRoot().getChildFile ("plugin_scan_crash.txt"), nullptr, false);

    tabs.addTab ("Audio",     tabColour, audio, true);
    tabs.addTab ("Plugins",   tabColour, plugins, true);
    tabs.addTab ("Recording", tabColour, makeRecordingPage(), true);
    tabs.addTab ("Folders",   tabColour, makeFoldersPage(), true);
    tabs.addTab ("About",     tabColour, makeAboutPage(), true);
    tabs.setTabBarDepth (30);
    addAndMakeVisible (tabs);

    setSize (720, 560);
}

SettingsComponent::~SettingsComponent() = default;

juce::File SettingsComponent::getRoot()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("KNG Studio");
}

juce::Component* SettingsComponent::makeRecordingPage()
{
    auto* page = new Page();

    page->items.add (makeText ("Recording latency", 16.0f, 1.0f));
    page->heights.add (24);

    page->items.add (makeText ("If your vocals land a little late or early against the beat, run the latency check. "
                               "It plays a click through your speakers, records it back, and lines up every future "
                               "recording automatically.", 13.0f, 0.65f));
    page->heights.add (60);

    auto* button = new juce::TextButton ("Run Latency Check...");
    button->onClick = [this] { if (onLatencyCheck) onLatencyCheck(); };
    page->items.add (button);
    page->heights.add (32);

    page->items.add (makeText ("Hearing your effects while you record: put your chain on the track, arm it, "
                               "and turn on Monitor in the transport bar.", 13.0f, 0.65f));
    page->heights.add (40);

    for (auto* c : page->items)
        page->addAndMakeVisible (c);

    return page;
}

juce::Component* SettingsComponent::makeFoldersPage()
{
    auto* page = new Page();

    struct Folder { const char* label; const char* sub; };
    const Folder folders[] = {
        { "Open Projects folder",  "" },
        { "Open Kits folder",      "Kits" },
        { "Open Templates folder", "Templates" }
    };

    page->items.add (makeText ("Everything lives in Documents\\KNG Studio. Drop sample folders into Kits "
                               "and they show up in the Browser.", 13.0f, 0.65f));
    page->heights.add (40);

    for (auto& f : folders)
    {
        auto folder = juce::String (f.sub).isEmpty() ? getRoot() : getRoot().getChildFile (f.sub);
        auto* button = new juce::TextButton (f.label);

        button->onClick = [folder]
        {
            folder.createDirectory();
            folder.startAsProcess();
        };

        page->items.add (button);
        page->heights.add (32);
    }

    for (auto* c : page->items)
        page->addAndMakeVisible (c);

    return page;
}

juce::Component* SettingsComponent::makeAboutPage()
{
    auto* page = new Page();

    page->items.add (makeText ("KNG Studio", 26.0f, 1.0f));
    page->heights.add (36);

    page->items.add (makeText ("Built with JUCE and Tracktion Engine.", 13.0f, 0.65f));
    page->heights.add (22);

    page->items.add (makeText ("Licensing: JUCE and Tracktion Engine are GPL unless you hold commercial licences. "
                               "Get both before selling or sharing KNG Studio.", 13.0f, 0.65f));
    page->heights.add (40);

    for (auto* c : page->items)
        page->addAndMakeVisible (c);

    return page;
}

void SettingsComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2124));
}

void SettingsComponent::resized()
{
    tabs.setBounds (getLocalBounds());
}
