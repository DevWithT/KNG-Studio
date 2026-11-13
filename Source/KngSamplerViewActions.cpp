#include "KngSamplerView.h"
#include "EditSession.h"
#include "TheoryHelper.h"
#include "KitBrowser.h"
#include "KngPopOut.h"

namespace te = tracktion;

static bool isAudioFile (const juce::File& f)
{
    return f.hasFileExtension ("wav;aif;aiff;flac;mp3;ogg");
}

//==============================================================================
void KngSamplerView::setProp (const juce::Identifier& id, const juce::var& value)
{
    if (auto* um = plugin.getUndoManager())
        um->beginNewTransaction();

    plugin.state.setProperty (id, value, plugin.getUndoManager());
    plugin.syncFromState();
    repaint();
}

float KngSamplerView::propFloat (const juce::Identifier& id, float fallback) const
{
    return (float) plugin.state.getProperty (id, fallback);
}

void KngSamplerView::loadFile (const juce::File& file)
{
    if (! isAudioFile (file))
        return;

    // Through the session when there is one, so the sample lands in the project like it does from the Rack
    if (session != nullptr)
    {
        for (int i = 0; i < session->getNumTracks(); ++i)
        {
            if (session->getTrack (i) == plugin.getOwnerTrack())
            {
                session->setSamplerSample (i, file, (int) plugin.state.getProperty (KngSamplerPlugin::rootProp, 60));
                peaksFile = juce::File();
                return;
            }
        }
    }

    plugin.loadSample (file);
    peaksFile = juce::File();
}

void KngSamplerView::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Load Sample", EditSession::getKitsFolder(),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              if (fc.getResult().existsAsFile())
                                  loadFile (fc.getResult());
                          });
}

void KngSamplerView::rebuildPeaks (int bars)
{
    peaks.clear();
    peaksFile = plugin.getSampleFile();
    auto buffer = plugin.getSampleForDisplay();
    peaksBuffer = buffer.get();

    if (buffer == nullptr || bars <= 0 || buffer->getNumSamples() == 0)
        return;

    const int len = buffer->getNumSamples();
    const int chans = buffer->getNumChannels();
    peaks.assign ((size_t) bars, 0.0f);
    float loudest = 0.0f;

    for (int x = 0; x < bars; ++x)
    {
        const int a = (int) ((juce::int64) x * len / bars);
        const int b = juce::jmax (a + 1, (int) ((juce::int64) (x + 1) * len / bars));
        float peak = 0.0f;

        for (int ch = 0; ch < chans; ++ch)
        {
            const float* d = buffer->getReadPointer (ch);

            for (int i = a; i < b && i < len; ++i)
                peak = juce::jmax (peak, std::abs (d[i]));
        }

        peaks[(size_t) x] = peak;
        loudest = juce::jmax (loudest, peak);
    }

    // Quiet samples still fill the screen
    if (loudest > 0.0f)
        for (auto& p : peaks)
            p /= loudest;
}

//==============================================================================
void KngSamplerView::timerCallback()
{
    popButton.setVisible (! KngPopOut::isPoppedOut (*this));

    const int bars = juce::jmax (0, (wave.getWidth() - 32) / 4);
    auto buffer = plugin.getSampleForDisplay();

    if (plugin.getSampleFile() != peaksFile || buffer.get() != peaksBuffer || (int) peaks.size() != bars)
        rebuildPeaks (bars);

    const int note = plugin.displayNote.load();

    if (note != shownNote)
    {
        shownNote = note;
        keyboard.repaint();
    }

    repaint (wave);
    repaint (header);
}

void KngSamplerView::handleNoteOn (juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity)
{
    if (auto* at = dynamic_cast<te::AudioTrack*> (plugin.getOwnerTrack()))
        at->playGuideNote (midiNoteNumber, te::MidiChannel (1), juce::roundToInt (velocity * 127.0f), false, true, false);
}

void KngSamplerView::handleNoteOff (juce::MidiKeyboardState*, int, int, float)
{
    if (auto* at = dynamic_cast<te::AudioTrack*> (plugin.getOwnerTrack()))
        at->turnOffGuideNotes (te::MidiChannel (1));
}

//==============================================================================
void KngSamplerView::mouseDown (const juce::MouseEvent& e)
{
    dragHandle = 0;

    if (! wave.contains (e.getPosition()))
        return;

    if (peaks.empty())
    {
        chooseFile();
        return;
    }

    if (! (bool) plugin.state.getProperty (KngSamplerPlugin::loopProp, false))
        return;

    const int xs = fracToX (propFloat (KngSamplerPlugin::loopStartProp, 0.4f));
    const int xe = fracToX (propFloat (KngSamplerPlugin::loopEndProp, 0.9f));

    if (std::abs (e.x - xs) <= 8)
        dragHandle = 1;
    else if (std::abs (e.x - xe) <= 8)
        dragHandle = 2;

    if (dragHandle != 0)
        if (auto* um = plugin.getUndoManager())
            um->beginNewTransaction();
}

void KngSamplerView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragHandle == 0)
        return;

    const float f = xToFrac (e.x);
    const float ls = propFloat (KngSamplerPlugin::loopStartProp, 0.4f);
    const float le = propFloat (KngSamplerPlugin::loopEndProp, 0.9f);

    // The limits stay in order even if a saved loop is odd
    if (dragHandle == 1)
        plugin.state.setProperty (KngSamplerPlugin::loopStartProp, juce::jlimit (0.0f, juce::jmax (0.0f, le - 0.02f), f), plugin.getUndoManager());
    else
        plugin.state.setProperty (KngSamplerPlugin::loopEndProp, juce::jlimit (juce::jmin (1.0f, ls + 0.02f), 1.0f, f), plugin.getUndoManager());

    plugin.syncFromState();
    repaint (wave);
}

void KngSamplerView::mouseUp (const juce::MouseEvent&)
{
    dragHandle = 0;
}

//==============================================================================
bool KngSamplerView::isInterestedInDragSource (const SourceDetails& details)
{
    juce::File f;

    return dynamic_cast<juce::FileTreeComponent*> (details.sourceComponent.get()) != nullptr
        || KitBrowser::getDraggedFile (details.description, f);
}

void KngSamplerView::itemDropped (const SourceDetails& details)
{
    hovering = false;
    juce::File f;

    if (KitBrowser::getDraggedFile (details.description, f))
        loadFile (f);
    else if (auto* tree = dynamic_cast<juce::FileTreeComponent*> (details.sourceComponent.get()))
        loadFile (tree->getSelectedFile());

    repaint();
}

bool KngSamplerView::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && isAudioFile (juce::File (files[0]));
}

void KngSamplerView::filesDropped (const juce::StringArray& files, int, int)
{
    hovering = false;

    if (! files.isEmpty())
        loadFile (juce::File (files[0]));

    repaint();
}
