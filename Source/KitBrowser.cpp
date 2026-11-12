#include "KitBrowser.h"
#include "EditSession.h"

static const char* audioPatterns = "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg";

static juce::File favoritesFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("KNG Studio").getChildFile ("favorites.txt");
}

//==============================================================================
SamplePreviewer::SamplePreviewer (juce::AudioDeviceManager& deviceManagerToUse)
    : deviceManager (deviceManagerToUse)
{
    formats.registerBasicFormats();
    player.setSource (&resampler);
    deviceManager.addAudioCallback (&player);
}

SamplePreviewer::~SamplePreviewer()
{
    transport.stop();
    transport.setSource (nullptr);
    player.setSource (nullptr);
    deviceManager.removeAudioCallback (&player);
}

void SamplePreviewer::play (const juce::File& file, double speedRatio)
{
    transport.stop();
    transport.setSource (nullptr);
    readerSource.reset();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
        return;

    const double sampleRate = reader->sampleRate;
    readerSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);

    // Above 1 plays faster (and higher), below 1 slower
    resampler.setResamplingRatio (juce::jlimit (0.25, 4.0, speedRatio));
    transport.setSource (readerSource.get(), 0, nullptr, sampleRate);
    transport.setPosition (0.0);
    transport.start();
}

//==============================================================================
bool KitBrowser::getDraggedFile (const juce::var& description, juce::File& out)
{
    const auto text = description.toString();

    if (! text.startsWith ("kngfile|"))
        return false;

    out = juce::File (text.substring (8));
    return out.existsAsFile();
}

double KitBrowser::bpmFromName (const juce::String& name)
{
    const auto lower = name.toLowerCase();
    const int idx = lower.indexOf ("bpm");

    if (idx < 0)
        return 0.0;

    int end = idx;

    while (end > 0 && (lower[end - 1] == ' ' || lower[end - 1] == '_' || lower[end - 1] == '-'))
        --end;

    int start = end;

    while (start > 0 && juce::CharacterFunctions::isDigit (lower[start - 1]))
        --start;

    const double bpm = lower.substring (start, end).getDoubleValue();
    return (bpm >= 50.0 && bpm <= 250.0) ? bpm : 0.0;
}

//==============================================================================
KitBrowser::KitBrowser (juce::AudioDeviceManager& deviceManager)
    : kitsFolder (EditSession::getKitsFolder()),
      previewer (deviceManager)
{
    scanThread.startThread (juce::Thread::Priority::low);
    contents.setDirectory (kitsFolder, true, true);

    // Lets you drag samples and kit folders onto the Rack and samplers
    tree.setDragAndDropDescription ("KngSample");
    tree.addListener (this);
    tree.setColour (juce::FileTreeComponent::backgroundColourId, juce::Colour (0xff1b1d20));

    addAndMakeVisible (title);
    title.setFont (juce::Font (juce::FontOptions (14.0f)));

    addAndMakeVisible (openFolderButton);
    openFolderButton.onClick = [this] { kitsFolder.startAsProcess(); };

    addAndMakeVisible (viewBox);
    viewBox.addItem ("All Sounds", 1);
    viewBox.addItem ("Favorites", 2);
    viewBox.setSelectedId (1, juce::dontSendNotification);
    viewBox.onChange = [this] { updateList(); };

    addAndMakeVisible (syncButton);
    syncButton.setToggleState (true, juce::dontSendNotification);
    syncButton.setTooltip ("Preview loops at your song's tempo (needs the tempo in the file name, like \"loop 140bpm\")");

    addAndMakeVisible (search);
    search.setTextToShowWhenEmpty ("Search sounds...", juce::Colours::white.withAlpha (0.4f));
    search.onTextChange = [this] { searchCountdown = 3; };

    addAndMakeVisible (tree);
    addChildComponent (list);
    list.setModel (this);
    list.setRowHeight (24);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1b1d20));

    loadFavorites();
    lastSignature = getFolderSignature();
    startTimerHz (10);
}

KitBrowser::~KitBrowser()
{
    stopTimer();
    list.setModel (nullptr);
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
    // Search waits until you stop typing for a moment
    if (searchCountdown > 0 && --searchCountdown == 0)
        updateList();

    if (--signatureCountdown <= 0)
    {
        signatureCountdown = 20;
        const auto sig = getFolderSignature();

        if (sig != lastSignature)
        {
            lastSignature = sig;
            allFilesDirty = true;
            contents.refresh();
            updateList();
        }
    }
}

//==============================================================================
void KitBrowser::preview (const juce::File& file)
{
    double ratio = 1.0;

    if (syncButton.getToggleState() && getProjectBpm)
    {
        const double fileBpm = bpmFromName (file.getFileNameWithoutExtension());
        const double songBpm = getProjectBpm();

        if (fileBpm > 0.0 && songBpm > 0.0)
            ratio = songBpm / fileBpm;
    }

    previewer.play (file, ratio);
}

void KitBrowser::updateList()
{
    const auto query = search.getText().trim();
    shown.clear();

    if (query.isNotEmpty())
    {
        if (allFilesDirty)
        {
            allFiles = kitsFolder.findChildFiles (juce::File::findFiles, true, audioPatterns);
            allFilesDirty = false;
        }

        for (auto& f : allFiles)
        {
            if (f.getFileName().containsIgnoreCase (query))
                shown.add (f);

            if (shown.size() >= 400)
                break;
        }
    }
    else if (viewBox.getSelectedId() == 2)
    {
        for (auto& f : favorites)
            if (f.existsAsFile())
                shown.add (f);
    }

    const bool showList = query.isNotEmpty() || viewBox.getSelectedId() == 2;
    list.setVisible (showList);
    tree.setVisible (! showList);
    list.updateContent();
    list.repaint();
}

void KitBrowser::showFileMenu (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    juce::Component::SafePointer<KitBrowser> safe (this);
    juce::PopupMenu menu;

    menu.addItem (isFavorite (file) ? "Remove from Favorites" : "Add to Favorites",
                  [safe, file] { if (safe != nullptr) safe->toggleFavorite (file); });
    menu.addItem ("Show in Explorer", [file] { file.revealToUser(); });
    menu.showMenuAsync ({});
}

//==============================================================================
void KitBrowser::loadFavorites()
{
    favorites.clear();
    juce::StringArray lines;
    lines.addLines (favoritesFile().loadFileAsString());

    for (auto& line : lines)
        if (line.trim().isNotEmpty())
            favorites.add (juce::File (line.trim()));
}

void KitBrowser::saveFavorites()
{
    juce::StringArray lines;

    for (auto& f : favorites)
        lines.add (f.getFullPathName());

    favoritesFile().getParentDirectory().createDirectory();
    favoritesFile().replaceWithText (lines.joinIntoString ("\n"));
}

bool KitBrowser::isFavorite (const juce::File& file) const
{
    return favorites.contains (file);
}

void KitBrowser::toggleFavorite (const juce::File& file)
{
    if (isFavorite (file))
        favorites.removeFirstMatchingValue (file);
    else
        favorites.add (file);

    saveFavorites();
    updateList();
}

//==============================================================================
void KitBrowser::selectionChanged()
{
    const auto file = tree.getSelectedFile();

    if (file.existsAsFile())
        preview (file);
}

void KitBrowser::fileClicked (const juce::File& file, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showFileMenu (file);
}

int KitBrowser::getNumRows()
{
    return shown.size();
}

void KitBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, shown.size()))
        return;

    const auto& f = shown.getReference (row);

    if (selected)
    {
        g.setColour (juce::Colour (0xff3a8ee6).withAlpha (0.25f));
        g.fillRoundedRectangle (juce::Rectangle<float> (2.0f, 1.0f, width - 4.0f, height - 2.0f), 4.0f);
    }

    auto r = juce::Rectangle<int> (6, 0, width - 12, height);

    if (isFavorite (f))
    {
        g.setColour (juce::Colour (0xffe6c23a));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85")), r.removeFromLeft (16), juce::Justification::centredLeft);
    }

    const double bpm = bpmFromName (f.getFileNameWithoutExtension());

    if (bpm > 0.0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText (juce::String (juce::roundToInt (bpm)) + " BPM", r.removeFromRight (52), juce::Justification::centredRight);
    }

    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText (f.getFileNameWithoutExtension(), r, juce::Justification::centredLeft, true);
}

void KitBrowser::selectedRowsChanged (int lastRowSelected)
{
    if (juce::isPositiveAndBelow (lastRowSelected, shown.size()))
        preview (shown[lastRowSelected]);
}

void KitBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && juce::isPositiveAndBelow (row, shown.size()))
        showFileMenu (shown[row]);
}

juce::var KitBrowser::getDragSourceDescription (const juce::SparseSet<int>& rows)
{
    if (rows.isEmpty() || ! juce::isPositiveAndBelow (rows[0], shown.size()))
        return {};

    return "kngfile|" + shown[rows[0]].getFullPathName();
}

juce::String KitBrowser::getTooltipForRow (int row)
{
    if (! juce::isPositiveAndBelow (row, shown.size()))
        return {};

    return shown[row].getParentDirectory().getFileName() + "   (right-click for favorites)";
}

//==============================================================================
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

    r.removeFromTop (4);
    auto second = r.removeFromTop (26);
    syncButton.setBounds (second.removeFromRight (70));
    second.removeFromRight (4);
    viewBox.setBounds (second);

    r.removeFromTop (4);
    search.setBounds (r.removeFromTop (26));
    r.removeFromTop (6);

    tree.setBounds (r);
    list.setBounds (r);
}
