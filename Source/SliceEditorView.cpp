#include "SliceEditorView.h"

#include <algorithm>
#include "SliceCuts.h"

namespace te = tracktion;

//==============================================================================
SliceEditorView::SliceEditorView (EditSession& sessionToUse, te::AudioClipBase& clipToUse)
    : session (sessionToUse), clip (&clipToUse)
{
    const auto pos = clip->getPosition();
    clipStart = pos.getStart().inSeconds();
    length = juce::jmax (0.05, pos.getLength().inSeconds());

    for (auto* c : std::initializer_list<juce::Component*> { &sensLabel, &sensitivity, &detectButton, &clearButton,
                                                             &infoLabel, &hintLabel, &rackButton, &padsButton, &gridBox, &splitBox,
                                                             &doneButton })
        addAndMakeVisible (c);

    sensLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    infoLabel.setFont (juce::Font (juce::FontOptions (14.0f)));

    hintLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    hintLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    hintLabel.setText ("Click to add a marker. Drag to move it. Right-click a marker to delete it.",
                       juce::dontSendNotification);

    sensitivity.setSliderStyle (juce::Slider::LinearHorizontal);
    sensitivity.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    sensitivity.setRange (0.0, 1.0, 0.01);
    sensitivity.setValue (0.5, juce::dontSendNotification);
    sensitivity.onDragEnd = [this] { detect(); };
    sensitivity.onValueChange = [this]
    {
        if (! sensitivity.isMouseButtonDown())
            detect();
    };

    detectButton.onClick = [this] { detect(); };
    clearButton.onClick  = [this] { cuts.clear(); updateInfo(); repaint(); };
    rackButton.onClick   = [this] { chopToRack(); };
    padsButton.onClick   = [this] { sendToPads(); };

    gridBox.setTextWhenNothingSelected ("Grid cut...");
    gridBox.setTooltip ("Cut the clip at every grid line, counted on the song's tempo. T means triplets.");

    for (int i = 0; i < (int) std::size (KngSliceCuts::gridChoices); ++i)
        gridBox.addItem (KngSliceCuts::gridChoices[i].name, i + 1);

    gridBox.onChange = [this]
    {
        const int id = gridBox.getSelectedId();

        if (id >= 1)
            gridCut (KngSliceCuts::gridChoices[id - 1].beats);

        gridBox.setSelectedId (0, juce::dontSendNotification);
    };

    splitBox.setTextWhenNothingSelected ("Split into...");
    splitBox.setTooltip ("Cut the clip into this many equal parts.");

    for (int n = 2; n <= 16; ++n)
        splitBox.addItem (juce::String (n) + " parts", n);

    splitBox.onChange = [this]
    {
        const int id = splitBox.getSelectedId();

        if (id >= 2)
            splitCut (id);

        splitBox.setSelectedId (0, juce::dontSendNotification);
    };
    doneButton.onClick   = [this] { close(); };

    loadPeaks();
    detect();
    setupTap();
    setupFx();
    setupTempo();
    setupLane();
    setSize (900, 460);
}

//==============================================================================
juce::Rectangle<int> SliceEditorView::getWaveArea() const
{
    return getLocalBounds().withTrimmedTop (toolbarHeight).withTrimmedBottom (bottomHeight).reduced (8, 0);
}

double SliceEditorView::xToTime (float x) const
{
    const auto wave = getWaveArea();
    const double frac = (x - (float) wave.getX()) / (double) juce::jmax (1, wave.getWidth());
    return juce::jlimit (0.0, length, frac * length);
}

float SliceEditorView::timeToX (double t) const
{
    const auto wave = getWaveArea();
    return (float) wave.getX() + (float) (t / length) * (float) wave.getWidth();
}

int SliceEditorView::markerAt (float x) const
{
    int best = -1;
    float bestDist = 7.0f;

    for (int i = 0; i < (int) cuts.size(); ++i)
    {
        const float d = std::abs (timeToX (cuts[(size_t) i]) - x);

        if (d < bestDist)
        {
            bestDist = d;
            best = i;
        }
    }

    return best;
}

std::vector<double> SliceEditorView::getFullCuts() const
{
    std::vector<double> full { clipStart };

    for (auto c : cuts)
        full.push_back (clipStart + c);

    full.push_back (clipStart + length);
    return full;
}

//==============================================================================
void SliceEditorView::loadPeaks()
{
    peaksLo.assign ((size_t) numPeaks, 0.0f);
    peaksHi.assign ((size_t) numPeaks, 0.0f);
    peakMax = 0.0f;

    const auto source = clip->getAudioFile().getFile();

    if (! source.existsAsFile())
        return;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (source));

    if (reader == nullptr || reader->numChannels == 0)
        return;

    const double offset = clip->getPosition().getOffset().inSeconds();
    const auto first = (juce::int64) (offset * reader->sampleRate);
    const auto total = (juce::int64) (length * reader->sampleRate);

    if (total <= 0)
        return;

    const int chunk = 4096;
    juce::AudioBuffer<float> buffer ((int) reader->numChannels, chunk);

    for (juce::int64 pos = 0; pos < total; pos += chunk)
    {
        const int n = (int) juce::jmin ((juce::int64) chunk, total - pos);
        buffer.clear();
        reader->read (&buffer, 0, n, first + pos, true, true);

        for (int i = 0; i < n; ++i)
        {
            float sample = 0.0f;

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                sample += buffer.getSample (ch, i);

            sample /= (float) buffer.getNumChannels();

            const int b = juce::jlimit (0, numPeaks - 1, (int) (((pos + i) * (juce::int64) numPeaks) / total));
            peaksLo[(size_t) b] = juce::jmin (peaksLo[(size_t) b], sample);
            peaksHi[(size_t) b] = juce::jmax (peaksHi[(size_t) b], sample);
            peakMax = juce::jmax (peakMax, std::abs (sample));
        }
    }
}

void SliceEditorView::detect()
{
    cuts = session.findHits (*clip, (float) sensitivity.getValue());
    updateInfo();
    repaint();
}

// Replaces the markers with cuts on a beat grid (counted on the song's tempo, from the clip start)
void SliceEditorView::gridCut (double beats)
{
    auto result = KngSliceCuts::grid (session, *clip, beats, 16);
    const bool tooMany = result.size() > 15;

    if (tooMany)
        result.resize (15);

    cuts = result;
    hintLabel.setText (tooMany ? "That grid makes more than 16 slices, so the cut stops at 16. Pick a bigger grid or use a shorter clip."
                               : "Click to add a marker. Drag to move it. Right-click a marker to delete it.",
                       juce::dontSendNotification);
    updateInfo();
    repaint();
}

// Replaces the markers with equal parts
void SliceEditorView::splitCut (int parts)
{
    cuts = KngSliceCuts::equalParts (length, parts);
    hintLabel.setText ("Click to add a marker. Drag to move it. Right-click a marker to delete it.", juce::dontSendNotification);
    updateInfo();
    repaint();
}

void SliceEditorView::updateInfo()
{
    syncSliceFx();
    const int n = (int) cuts.size() + 1;
    infoLabel.setText (juce::String (n) + (n == 1 ? " slice" : " slices") + " (16 max)", juce::dontSendNotification);
}

//==============================================================================
void SliceEditorView::mouseDown (const juce::MouseEvent& e)
{
    dragging = -1;

    if (! getWaveArea().contains (e.getPosition()))
        return;

    const int m = markerAt ((float) e.x);

    if (m < 0 && ! e.mods.isPopupMenu() && selectSliceAt (e.getPosition()))
        return;

    if (e.mods.isPopupMenu())
    {
        if (m >= 0)
        {
            cuts.erase (cuts.begin() + m);
            updateInfo();
            repaint();
        }

        return;
    }

    if (m >= 0)
    {
        dragging = m;
        repaint();
        return;
    }

    if (cuts.size() >= 15)
        return;

    const double t = juce::jlimit (0.01, length - 0.01, xToTime ((float) e.x));
    cuts.push_back (t);
    std::sort (cuts.begin(), cuts.end());
    dragging = (int) (std::find (cuts.begin(), cuts.end(), t) - cuts.begin());

    updateInfo();
    repaint();
}

void SliceEditorView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0 || dragging >= (int) cuts.size())
        return;

    const double lo = dragging > 0 ? cuts[(size_t) dragging - 1] + 0.01 : 0.01;
    const double hi = dragging + 1 < (int) cuts.size() ? cuts[(size_t) dragging + 1] - 0.01 : length - 0.01;

    cuts[(size_t) dragging] = juce::jlimit (lo, juce::jmax (lo, hi), xToTime ((float) e.x));

    if (prevCuts.size() == cuts.size())
        prevCuts[(size_t) dragging] = cuts[(size_t) dragging];
    repaint();
}

void SliceEditorView::mouseUp (const juce::MouseEvent&)
{
    dragging = -1;
    repaint();
}

//==============================================================================
void SliceEditorView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2124));

    g.setColour (juce::Colour (0xff2b2f33));
    g.fillRect (getLocalBounds().removeFromTop (toolbarHeight));
    g.fillRect (getLocalBounds().removeFromBottom (bottomHeight));

    const auto wave = getWaveArea();

    if (wave.getWidth() < 2)
        return;

    // Slice shading and numbers
    std::vector<double> edges { 0.0 };

    for (auto c : cuts)
        edges.push_back (c);

    edges.push_back (length);

    g.setFont (juce::Font (juce::FontOptions (12.0f)));

    for (size_t i = 0; i + 1 < edges.size(); ++i)
    {
        const float x1 = timeToX (edges[i]);
        const float x2 = timeToX (edges[i + 1]);

        g.setColour (i % 2 == 0 ? juce::Colour (0xff2a3440) : juce::Colour (0xff222a33));
        g.fillRect (x1, (float) wave.getY(), x2 - x1, (float) wave.getHeight());

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawText (juce::String ((int) i + 1),
                    juce::Rectangle<float> (x1 + 6.0f, (float) wave.getY() + 10.0f, 30.0f, 16.0f),
                    juce::Justification::centredLeft);
    }

    // Waveform
    if (peakMax > 0.0f)
    {
        const float midY = (float) wave.getCentreY();
        const float half = (float) wave.getHeight() * 0.42f;

        g.setColour (juce::Colour (0xff6fb0ff));

        for (int x = 0; x < wave.getWidth(); ++x)
        {
            const int b0 = x * numPeaks / wave.getWidth();
            const int b1 = juce::jmin (numPeaks, juce::jmax (b0 + 1, (x + 1) * numPeaks / wave.getWidth()));
            float lo = 0.0f, hi = 0.0f;

            for (int b = b0; b < b1; ++b)
            {
                lo = juce::jmin (lo, peaksLo[(size_t) b]);
                hi = juce::jmax (hi, peaksHi[(size_t) b]);
            }

            g.drawVerticalLine (wave.getX() + x, midY - hi / peakMax * half, midY - lo / peakMax * half + 1.0f);
        }
    }

    drawSliceExtras (g, wave);
    drawPlayhead (g, wave);

    // Markers
    for (int i = 0; i < (int) cuts.size(); ++i)
    {
        const float x = timeToX (cuts[(size_t) i]);

        g.setColour (i == dragging ? juce::Colour (0xffffd24a) : juce::Colours::white);
        g.drawLine (x, (float) wave.getY(), x, (float) wave.getBottom(), i == dragging ? 2.5f : 1.5f);
        g.fillRect (x - 4.0f, (float) wave.getY(), 8.0f, 8.0f);
    }
}

void SliceEditorView::resized()
{
    auto toolbar = getLocalBounds().removeFromTop (toolbarHeight).reduced (8, 8);
    auto top = toolbar.removeFromTop (28);
    toolbar.removeFromTop (8);
    layoutTapRow (toolbar.removeFromTop (28));
    sensLabel.setBounds (top.removeFromLeft (80));
    sensitivity.setBounds (top.removeFromLeft (180));
    top.removeFromLeft (8);
    detectButton.setBounds (top.removeFromLeft (100));
    top.removeFromLeft (6);
    clearButton.setBounds (top.removeFromLeft (70));
    top.removeFromLeft (12);
    gridBox.setBounds (top.removeFromLeft (120));
    top.removeFromLeft (6);
    splitBox.setBounds (top.removeFromLeft (120));
    top.removeFromLeft (12);
    infoLabel.setBounds (top);

    auto bottom = getLocalBounds().removeFromBottom (bottomHeight).reduced (8, 8);
    layoutFxRow (bottom.removeFromTop (28));
    bottom.removeFromTop (8);
    doneButton.setBounds (bottom.removeFromRight (80));
    bottom.removeFromRight (6);
    padsButton.setBounds (bottom.removeFromRight (130));
    bottom.removeFromRight (6);
    rackButton.setBounds (bottom.removeFromRight (120));
    bottom.removeFromRight (6);
    laneButton.setBounds (bottom.removeFromRight (130));
    bottom.removeFromRight (10);
    hintLabel.setBounds (bottom);
}

//==============================================================================
void SliceEditorView::close()
{
    if (auto* w = findParentComponentOfClass<juce::DialogWindow>())
        w->exitModalState (0);
}

void SliceEditorView::chopToRack()
{
    session.setSliceFx (sliceFx);
    session.setSliceSourceBpm (matchToggle.getToggleState() ? sourceBpm.getValue() : 0.0);

    const int chopped = session.chopClipAtCuts (*clip, getFullCuts());
    session.setSliceSourceBpm (0.0);
    session.setSliceFx ({});

    if (chopped >= 0)
        close();
    else
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Slice Editor",
                                                "Couldn't slice this clip.");
}

void SliceEditorView::sendToPads()
{
    juce::PopupMenu menu;

    for (int i = 0; i < session.getNumTracks(); ++i)
        if (session.isDrumTrack (i))
            if (auto* t = session.getTrack (i))
                menu.addItem (i + 1, t->getName());

    if (menu.getNumItems() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Slice Editor",
                                                "Add a Drum Kit first, then send the slices to its pads.");
        return;
    }

    juce::Component::SafePointer<SliceEditorView> safe (this);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&padsButton),
                        [safe] (int result)
                        {
                            if (safe == nullptr || result <= 0)
                                return;

                            const int trackIndex = result - 1;
                            safe->session.setSliceFx (safe->sliceFx);
                            const int loaded = safe->session.slicesToPads (*safe->clip, safe->getFullCuts(), trackIndex);
                            safe->session.setSliceFx ({});

                            if (loaded < 0)
                            {
                                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                        "Slice Editor", "Couldn't send the slices to the pads.");
                                return;
                            }

                            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Slice Editor",
                                                                    juce::String (loaded) + " slices loaded onto pads 1 to "
                                                                      + juce::String (loaded) + ".");
                        });
}

//==============================================================================
void showSliceEditor (EditSession& session, te::AudioClipBase& clip)
{
    auto view = std::make_unique<SliceEditorView> (session, clip);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (view.release());
    o.dialogTitle = "Slice Editor";
    o.dialogBackgroundColour = juce::Colour (0xff1e2124);
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}
