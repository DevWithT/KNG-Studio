#include "KngEqView.h"
#include "KngEqPlugin.h"

namespace te = tracktion;

static constexpr double gainRange = 24.0;

static juce::Colour bandColour (int band)
{
    static const juce::uint32 colours[KngEq::numBands] = {
        0xffe6503a, 0xffe6a23a, 0xffd4d43a, 0xff5ad45a, 0xff3ad4c0, 0xff3a8ee6, 0xff8e6ae6, 0xffe65ab4
    };

    return juce::Colour (colours[band]);
}

static juce::String freqText (double f)
{
    if (f >= 1000.0)
        return juce::String (f / 1000.0, f >= 10000.0 ? 1 : 2) + " kHz";

    return juce::String (juce::roundToInt (f)) + " Hz";
}

static juce::String gainText (double v)
{
    return (v > 0.0 ? "+" : "") + juce::String (v, 1) + " dB";
}

static bool hasGain (int type)
{
    return type == KngEq::lowShelf || type == KngEq::bell || type == KngEq::highShelf;
}

static bool isCut (int type)
{
    return type == KngEq::lowCut || type == KngEq::highCut;
}

//==============================================================================
KngEqView::KngEqView (KngEqPlugin& p)
    : plugin (p), pluginRef (&p)
{
    for (auto* c : std::initializer_list<juce::Component*> { &bandLabel, &onButton, &typeBox, &slopeBox,
                                                             &freqKnob, &gainKnob, &qKnob, &outKnob,
                                                             &freqLabel, &gainLabel, &qLabel, &outLabel })
        addAndMakeVisible (c);

    bandLabel.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));

    for (auto* l : { &freqLabel, &gainLabel, &qLabel, &outLabel })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (juce::Font (juce::FontOptions (11.0f)));
        l->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    }

    for (int i = 0; i < KngEq::typeNames().size(); ++i)
        typeBox.addItem (KngEq::typeNames()[i], i + 1);

    for (int i = 0; i < KngEq::slopeNames().size(); ++i)
        slopeBox.addItem (KngEq::slopeNames()[i], i + 1);

    typeBox.onChange = [this]
    {
        if (selected >= 0)
        {
            beginEdit();
            setValue (selected, KngEq::bandType, (float) (typeBox.getSelectedId() - 1));
        }
    };

    slopeBox.onChange = [this]
    {
        if (selected >= 0)
        {
            beginEdit();
            setValue (selected, KngEq::bandSlope, (float) (slopeBox.getSelectedId() - 1));
        }
    };

    onButton.onClick = [this]
    {
        if (selected >= 0)
        {
            beginEdit();
            setValue (selected, KngEq::bandOn, onButton.getToggleState() ? 1.0f : 0.0f);
        }
    };

    auto setupKnob = [this] (juce::Slider& s, double min, double max, double mid, double def,
                             std::function<juce::String (double)> text)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
        s.setRange (min, max, 0.0);

        if (mid > min && mid < max)
            s.setSkewFactorFromMidPoint (mid);

        s.setDoubleClickReturnValue (true, def);
        s.textFromValueFunction = std::move (text);
        s.onDragStart = [this] { beginEdit(); };
    };

    setupKnob (freqKnob, 20.0, 20000.0, 1000.0, 1000.0, freqText);
    setupKnob (gainKnob, -gainRange, gainRange, 0.0, 0.0, gainText);
    setupKnob (qKnob, 0.1, 18.0, 1.0, 1.0, [] (double v) { return juce::String (v, 2); });
    setupKnob (outKnob, -24.0, 24.0, 0.0, 0.0, gainText);

    freqKnob.onValueChange = [this] { if (selected >= 0) setValue (selected, KngEq::bandFreq, (float) freqKnob.getValue()); };
    gainKnob.onValueChange = [this] { if (selected >= 0) setValue (selected, KngEq::bandGain, (float) gainKnob.getValue()); };
    qKnob.onValueChange    = [this] { if (selected >= 0) setValue (selected, KngEq::bandQ, (float) qKnob.getValue()); };
    outKnob.onValueChange  = [this] { plugin.getParam (KngEq::outputParam)->setParameter ((float) outKnob.getValue(), juce::sendNotification); };

    selectBand (-1);
    setSize (900, 380);
    startTimerHz (30);
}

KngEqView::~KngEqView()
{
    stopTimer();
}

//==============================================================================
void KngEqView::beginEdit()
{
    if (auto* um = plugin.getUndoManager())
        um->beginNewTransaction();
}

float KngEqView::value (int band, int p) const
{
    return plugin.getParam (KngEq::index (band, p))->getCurrentValue();
}

void KngEqView::setValue (int band, int p, float v)
{
    plugin.getParam (KngEq::index (band, p))->setParameter (v, juce::sendNotification);
}

float KngEqView::freqToX (double f) const
{
    return graph.getX() + graph.getWidth() * (float) (std::log (f / 20.0) / std::log (1000.0));
}

double KngEqView::xToFreq (float x) const
{
    const double t = (x - graph.getX()) / juce::jmax (1.0f, graph.getWidth());
    return juce::jlimit (20.0, 20000.0, 20.0 * std::pow (1000.0, t));
}

float KngEqView::gainToY (double db) const
{
    const float half = graph.getHeight() * 0.5f - 10.0f;
    return graph.getCentreY() - (float) (db / gainRange) * half;
}

double KngEqView::yToGain (float y) const
{
    const float half = graph.getHeight() * 0.5f - 10.0f;
    return juce::jlimit (-gainRange, gainRange, (graph.getCentreY() - y) / juce::jmax (1.0f, half) * gainRange);
}

juce::Point<float> KngEqView::handlePos (int band) const
{
    const int type = juce::roundToInt (value (band, KngEq::bandType));
    return { freqToX (value (band, KngEq::bandFreq)), gainToY (hasGain (type) ? value (band, KngEq::bandGain) : 0.0) };
}

int KngEqView::bandAt (juce::Point<float> p) const
{
    for (int b = KngEq::numBands; --b >= 0;)
        if (value (b, KngEq::bandOn) > 0.5f && handlePos (b).getDistanceFrom (p) <= 10.0f)
            return b;

    return -1;
}

void KngEqView::selectBand (int band)
{
    selected = band;
    syncControls();
    repaint();
}

//==============================================================================
void KngEqView::syncControls()
{
    const bool has = selected >= 0 && value (selected, KngEq::bandOn) > 0.5f;

    if (selected >= 0 && ! has)
        selected = -1;

    for (auto* c : std::initializer_list<juce::Component*> { &onButton, &typeBox, &slopeBox, &freqKnob, &gainKnob,
                                                             &qKnob, &freqLabel, &gainLabel, &qLabel })
        c->setVisible (has);

    if (! outKnob.isMouseButtonDown())
        outKnob.setValue (plugin.getParam (KngEq::outputParam)->getCurrentValue(), juce::dontSendNotification);

    if (! has)
    {
        bandLabel.setText ("No band selected", juce::dontSendNotification);
        bandLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.5f));
        return;
    }

    const int type = juce::roundToInt (value (selected, KngEq::bandType));

    bandLabel.setText ("Band " + juce::String (selected + 1), juce::dontSendNotification);
    bandLabel.setColour (juce::Label::textColourId, bandColour (selected));

    slopeBox.setVisible (isCut (type));
    gainKnob.setEnabled (hasGain (type));

    if (! typeBox.isPopupActive())
        typeBox.setSelectedId (type + 1, juce::dontSendNotification);

    if (! slopeBox.isPopupActive())
        slopeBox.setSelectedId (juce::roundToInt (value (selected, KngEq::bandSlope)) + 1, juce::dontSendNotification);

    onButton.setToggleState (true, juce::dontSendNotification);

    if (! freqKnob.isMouseButtonDown()) freqKnob.setValue (value (selected, KngEq::bandFreq), juce::dontSendNotification);
    if (! gainKnob.isMouseButtonDown()) gainKnob.setValue (value (selected, KngEq::bandGain), juce::dontSendNotification);
    if (! qKnob.isMouseButtonDown())    qKnob.setValue (value (selected, KngEq::bandQ), juce::dontSendNotification);
}

void KngEqView::updateSpectrum()
{
    const int n = plugin.pullAnalyserSamples (pullBuffer.data(), (int) pullBuffer.size());

    for (int i = 0; i < n; ++i)
    {
        ring[(size_t) ringPos] = pullBuffer[(size_t) i];
        ringPos = (ringPos + 1) % fftSize;
    }

    if (n > 0)
    {
        for (int i = 0; i < fftSize; ++i)
            fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % fftSize)];

        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());
    }

    // Rise instantly, fall slowly, so it reads like a meter
    for (int i = 0; i < fftSize / 2; ++i)
    {
        const float db = n > 0 ? juce::Decibels::gainToDecibels (fftData[(size_t) i] / (fftSize * 0.25f), -100.0f) : -100.0f;
        auto& s = spectrumDb[(size_t) i];
        s = db > s ? db : s - 1.2f;
    }
}

void KngEqView::timerCallback()
{
    updateSpectrum();
    syncControls();
    repaint (graph.toNearestInt().expanded (2));
}

//==============================================================================
void KngEqView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d20));

    g.setColour (juce::Colour (0xff141618));
    g.fillRoundedRectangle (graph, 6.0f);

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (graph.toNearestInt());

    const double sr = plugin.getSampleRate() > 0.0 ? plugin.getSampleRate() : 48000.0;
    g.setFont (juce::Font (juce::FontOptions (10.0f)));

    // Grid
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = freqToX (f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawVerticalLine ((int) x, graph.getY(), graph.getBottom());
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawText (freqText (f), (int) x + 3, (int) graph.getBottom() - 14, 60, 12, juce::Justification::left);
    }

    for (int db = -18; db <= 18; db += 6)
    {
        const float y = gainToY (db);
        g.setColour (juce::Colours::white.withAlpha (db == 0 ? 0.18f : 0.06f));
        g.drawHorizontalLine ((int) y, graph.getX(), graph.getRight());

        if (db != 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.drawText ((db > 0 ? "+" : "") + juce::String (db), (int) graph.getX() + 4, (int) y - 6, 30, 12,
                        juce::Justification::left);
        }
    }

    // Live spectrum
    juce::Path spectrum;
    float lastX = graph.getX();

    for (float x = graph.getX(); x <= graph.getRight(); x += 2.0f)
    {
        const double bin = xToFreq (x) / sr * fftSize;
        const int i0 = (int) bin;

        if (i0 >= fftSize / 2 - 1)
            break;

        const float frac = (float) (bin - i0);
        const float db = spectrumDb[(size_t) i0] + frac * (spectrumDb[(size_t) i0 + 1] - spectrumDb[(size_t) i0]);
        const float y = graph.getBottom() - juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 90.0f) * graph.getHeight();

        if (spectrum.isEmpty())
            spectrum.startNewSubPath (x, graph.getBottom());

        spectrum.lineTo (x, y);
        lastX = x;
    }

    if (! spectrum.isEmpty())
    {
        spectrum.lineTo (lastX, graph.getBottom());
        spectrum.closeSubPath();
        g.setColour (juce::Colour (0xff3a8ee6).withAlpha (0.12f));
        g.fillPath (spectrum);
        g.setColour (juce::Colour (0xff3a8ee6).withAlpha (0.3f));
        g.strokePath (spectrum, juce::PathStrokeType (1.0f));
    }

    // Each band's filter, then the total curve
    std::array<std::array<KngEq::Coefs, 4>, KngEq::numBands> bandCoefs {};
    std::array<int, KngEq::numBands> bandStages {};
    std::array<bool, KngEq::numBands> enabled {};

    for (int b = 0; b < KngEq::numBands; ++b)
    {
        enabled[(size_t) b] = value (b, KngEq::bandOn) > 0.5f;

        if (enabled[(size_t) b])
            bandStages[(size_t) b] = KngEq::makeBand (juce::roundToInt (value (b, KngEq::bandType)),
                                                      juce::roundToInt (value (b, KngEq::bandSlope)),
                                                      value (b, KngEq::bandFreq), value (b, KngEq::bandGain),
                                                      value (b, KngEq::bandQ), sr, bandCoefs[(size_t) b]);
    }

    auto bandDb = [&] (int b, double f)
    {
        double db = 0.0;

        for (int s = 0; s < bandStages[(size_t) b]; ++s)
            db += KngEq::magnitudeDb (bandCoefs[(size_t) b][(size_t) s], f, sr);

        return db;
    };

    for (int b = 0; b < KngEq::numBands; ++b)
    {
        if (! enabled[(size_t) b])
            continue;

        juce::Path curve;

        for (float x = graph.getX(); x <= graph.getRight(); x += 2.0f)
        {
            const float y = gainToY (juce::jlimit (-30.0, 30.0, bandDb (b, xToFreq (x))));

            if (curve.isEmpty())
                curve.startNewSubPath (x, y);
            else
                curve.lineTo (x, y);
        }

        g.setColour (bandColour (b).withAlpha (b == selected ? 0.8f : 0.4f));
        g.strokePath (curve, juce::PathStrokeType (1.2f));
    }

    juce::Path total;

    for (float x = graph.getX(); x <= graph.getRight(); x += 2.0f)
    {
        double db = 0.0;
        const double f = xToFreq (x);

        for (int b = 0; b < KngEq::numBands; ++b)
            if (enabled[(size_t) b])
                db += bandDb (b, f);

        const float y = gainToY (juce::jlimit (-30.0, 30.0, db));

        if (total.isEmpty())
            total.startNewSubPath (x, y);
        else
            total.lineTo (x, y);
    }

    g.setColour (juce::Colours::white.withAlpha (0.95f));
    g.strokePath (total, juce::PathStrokeType (2.2f));

    // Band dots
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    bool anyEnabled = false;

    for (int b = 0; b < KngEq::numBands; ++b)
    {
        if (! enabled[(size_t) b])
            continue;

        anyEnabled = true;
        const auto pos = handlePos (b);
        const auto dot = juce::Rectangle<float> (18.0f, 18.0f).withCentre (pos);

        g.setColour (bandColour (b));
        g.fillEllipse (dot);

        if (b == selected)
        {
            g.setColour (juce::Colours::white);
            g.drawEllipse (dot.expanded (3.0f), 2.0f);
        }

        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.drawText (juce::String (b + 1), dot.toNearestInt(), juce::Justification::centred);
    }

    if (! anyEnabled)
    {
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        g.drawText ("Double-click to add a band. Drag to move it, scroll to change its width, right-click for options.",
                    graph.toNearestInt().reduced (20), juce::Justification::centred);
    }
}

void KngEqView::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto controls = r.removeFromBottom (88);
    r.removeFromBottom (6);
    graph = r.toFloat();

    auto left = controls.removeFromLeft (150);
    bandLabel.setBounds (left.removeFromTop (26));
    left.removeFromTop (4);
    onButton.setBounds (left.removeFromTop (26).withWidth (70));

    controls.removeFromLeft (8);
    auto typeCol = controls.removeFromLeft (130);
    typeBox.setBounds (typeCol.removeFromTop (26));
    typeCol.removeFromTop (6);
    slopeBox.setBounds (typeCol.removeFromTop (26));

    controls.removeFromLeft (12);

    auto knob = [&controls] (juce::Label& l, juce::Slider& s)
    {
        auto col = controls.removeFromLeft (84);
        l.setBounds (col.removeFromTop (16));
        s.setBounds (col);
        controls.removeFromLeft (4);
    };

    knob (freqLabel, freqKnob);
    knob (gainLabel, gainKnob);
    knob (qLabel, qKnob);

    auto outCol = controls.removeFromRight (84);
    outLabel.setBounds (outCol.removeFromTop (16));
    outKnob.setBounds (outCol);
}

//==============================================================================
void KngEqView::mouseDown (const juce::MouseEvent& e)
{
    dragging = -1;

    if (! graph.contains (e.position))
        return;

    const int b = bandAt (e.position);

    if (e.mods.isPopupMenu())
    {
        if (b >= 0)
            showBandMenu (b);

        return;
    }

    if (b >= 0)
    {
        selectBand (b);
        dragging = b;
        beginEdit();
    }
}

void KngEqView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;

    const int type = juce::roundToInt (value (dragging, KngEq::bandType));
    setValue (dragging, KngEq::bandFreq, (float) xToFreq (e.position.x));

    if (hasGain (type))
        setValue (dragging, KngEq::bandGain, (float) yToGain (e.position.y));

    repaint();
}

void KngEqView::mouseUp (const juce::MouseEvent&)
{
    dragging = -1;
}

void KngEqView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! graph.contains (e.position))
        return;

    // Double-click a dot to remove that band
    const int hit = bandAt (e.position);

    if (hit >= 0)
    {
        beginEdit();
        setValue (hit, KngEq::bandOn, 0.0f);
        selectBand (-1);
        return;
    }

    // Double-click empty space to add a band there
    const double f = xToFreq (e.position.x);
    const int type = f < 40.0 ? KngEq::lowCut : f > 15000.0 ? KngEq::highCut : KngEq::bell;

    std::vector<int> order;

    if (type == KngEq::lowCut)       order = { 0, 1, 2, 3, 4, 5, 6, 7 };
    else if (type == KngEq::highCut) order = { 7, 6, 5, 4, 3, 2, 1, 0 };
    else                             order = { 2, 3, 4, 5, 1, 6, 0, 7 };

    int band = -1;

    for (int b : order)
    {
        if (value (b, KngEq::bandOn) < 0.5f)
        {
            band = b;
            break;
        }
    }

    if (band < 0)
        return;   // all 8 bands in use

    beginEdit();
    setValue (band, KngEq::bandType, (float) type);
    setValue (band, KngEq::bandFreq, (float) f);
    setValue (band, KngEq::bandGain, type == KngEq::bell ? (float) yToGain (e.position.y) : 0.0f);
    setValue (band, KngEq::bandQ, type == KngEq::bell ? 1.0f : 0.707f);
    setValue (band, KngEq::bandSlope, 0.0f);
    setValue (band, KngEq::bandOn, 1.0f);
    selectBand (band);
}

void KngEqView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    int b = bandAt (e.position);

    if (b < 0)
        b = selected;

    if (b < 0 || wheel.deltaY == 0.0f)
        return;

    // Scroll up for narrower, down for wider
    const float q = value (b, KngEq::bandQ) * (wheel.deltaY > 0.0f ? 1.15f : 1.0f / 1.15f);
    setValue (b, KngEq::bandQ, juce::jlimit (0.1f, 18.0f, q));
    repaint();
}

void KngEqView::showBandMenu (int band)
{
    juce::Component::SafePointer<KngEqView> safe (this);
    const int type = juce::roundToInt (value (band, KngEq::bandType));
    juce::PopupMenu menu;

    for (int t = 0; t < KngEq::typeNames().size(); ++t)
    {
        menu.addItem (KngEq::typeNames()[t], true, t == type, [safe, band, t]
        {
            if (safe == nullptr) return;
            safe->beginEdit();
            safe->setValue (band, KngEq::bandType, (float) t);
        });
    }

    if (isCut (type))
    {
        menu.addSeparator();
        const int slope = juce::roundToInt (value (band, KngEq::bandSlope));

        for (int s = 0; s < KngEq::slopeNames().size(); ++s)
        {
            menu.addItem (KngEq::slopeNames()[s], true, s == slope, [safe, band, s]
            {
                if (safe == nullptr) return;
                safe->beginEdit();
                safe->setValue (band, KngEq::bandSlope, (float) s);
            });
        }
    }

    menu.addSeparator();
    menu.addItem ("Remove Band", [safe, band]
    {
        if (safe == nullptr) return;
        safe->beginEdit();
        safe->setValue (band, KngEq::bandOn, 0.0f);
        safe->selectBand (-1);
    });

    menu.showMenuAsync ({});
}
