#include "KitBrowser.h"
#include "EditSession.h"

//==============================================================================
SamplePreviewer::SamplePreviewer (juce::AudioDeviceManager& deviceManagerToUse)
    : deviceManager (deviceManagerToUse)
{
    formats.registerBasicFormats();
    player.setSource (&transport);
    deviceManager.addAudioCallback (&player);
}

SamplePreviewer::~SamplePreviewer()
{
    transport.stop();
    transport.setSource (nullptr);
    player.setSource (nullptr);
    deviceManager.removeAudioCallback (&player);
}

void SamplePreviewer::play (const juce::File& file)
{
    transport.stop();
    transport.setSource (nullptr);
    readerSource.reset();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
        return;

    const double sampleRate = reader->sampleRate;
    readerSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);

    transport.setSource (readerSource.get(), 0, nullptr, sampleRate);
    transport.setPosition (0.0);
    transport.start();
}

//==============================================================================
KitBrowser::KitBrowser (juce::AudioDeviceManager& deviceManager)
    : kitsFolder (EditSession::getKitsFolder()),
      previewer (deviceManager)
{
    scanThread.startThread (juce::Thread::Priority::low);
    contents.setDirectory (kitsFolder, true, true);

    // Lets you drag samples and kit folders onto the Drum Rack
    tree.setDragAndDropDescription ("KngSample");
    tree.addListener (this);
    tree.setColour (juce::FileTreeComponent::backgroundColourId, juce::Colour (0xff1b1d20));

    addAndMakeVisible (title);
    title.setFont (juce::Font (juce::FontOptions (14.0f)));

    addAndMakeVisible (openFolderButton);
    openFolderButton.onClick = [this] { kitsFolder.startAsProcess(); };

    addAndMakeVisible (tree);

    lastSignature = getFolderSignature();
    startTimer (2000);
}

KitBrowser::~KitBrowser()
{
    stopTimer();
    tree.removeListener (this);
    scanThread.stopThread (2000);
}

juce::String KitBrowser::getFolderSignature() const
{
    // Names and dates two levels deep, cheap enough to check every couple of seconds
    juce::String sig;

    for (auto& top : kitsFolder.findChildFiles (juce::File::findFilesAndDirectories, false))
    {
        sig << top.getFileName() << top.getLastModificationTime().toMilliseconds() << ";";

        if (top.isDirectory())
            for (auto& inner : top.findChildFiles (juce::File::findFilesAndDirectories, false))
                sig << inner.getFileName() << ",";
    }

    return sig;
}

void KitBrowser::timerCallback()
{
    const auto sig = getFolderSignature();

    if (sig != lastSignature)
    {
        lastSignature = sig;
        contents.refresh();
    }
}

void KitBrowser::selectionChanged()
{
    const auto file = tree.getSelectedFile();

    if (file.existsAsFile())
        previewer.play (file);
}

void KitBrowser::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff24282c));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
}

void KitBrowser::resized()
{
    auto r = getLocalBounds().reduced (6);

    auto top = r.removeFromTop (26);
    openFolderButton.setBounds (top.removeFromRight (100));
    title.setBounds (top);

    r.removeFromTop (6);
    tree.setBounds (r);
}
