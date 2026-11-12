#include "KngSamplerView.h"
#include "EditSession.h"
#include "TheoryHelper.h"
#include "KitBrowser.h"
#include "KngPopOut.h"

namespace te = tracktion;

static const juce::Colour violet (0xff9b7bff), lilac (0xffb9a1ff), deepViolet (0xff7a62d6), rootPink (0xffff9bd2),
                          paper (0xfff1ecff), muted (0xff6d6684), ink (0xff130c2b);

static bool isAudioFile (const juce::File& f)
{
    return f.hasFileExtension ("wav;aif;aiff;flac;mp3;ogg");
}

static KngFaceStyle makeSamplerStyle()
{
    KngFaceStyle s;
    s.accent    = violet;
    s.bodyLight = juce::Colour (0xff34303f);
    s.bodyDark  = juce::Colour (0xff14121a);
    s.pointer   = paper;
    s.track     = juce::Colours::white.withAlpha (0.07f);
    s.label     = juce::Colour (0xff9a93b0);
    s.value     = juce::Colour (0xffc4b2ff);
    s.panel     = juce::Colour (0xff1a1722);
    s.outline   = juce::Colour (0xff2a2635);
    s.labelFace = KngFonts::sora;
    s.valueFace = KngFonts::mono;
    s.menuFace  = KngFonts::mono;
    s.upperCaseLabels = false;
    return s;
}

namespace
{
    /** A knob bound to one of the Sampler's settings. */
    class PropKnob : public juce::Component,
                     private juce::Timer
    {
    public:
        PropKnob (KngSamplerPlugin& p, const juce::Identifier& idToUse, const juce::String& labelText,
                  double min, double max, double mid, double step, double def,
                  std::function<juce::String (double)> formatter, const KngFaceStyle& s)
            : plugin (p), pluginRef (&p), id (idToUse), label (labelText), fallback (def),
              whole (step >= 1.0), format (std::move (formatter)), style (s)
        {
            addAndMakeVisible (knob);
            knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            knob.setRange (min, max, step);

            if (mid > min && mid < max)
                knob.setSkewFactorFromMidPoint (mid);

            knob.setDoubleClickReturnValue (true, def);
            knob.setValue (read(), juce::dontSendNotification);
            knob.setTooltip (label + " (double-click to reset)");

            knob.onDragStart = [this]
            {
                if (auto* um = plugin.getUndoManager())
                    um->beginNewTransaction();
            };

            knob.onValueChange = [this]
            {
                const double v = knob.getValue();
                plugin.state.setProperty (id, whole ? juce::var (juce::roundToInt (v)) : juce::var (v), plugin.getUndoManager());
                plugin.syncFromState();
                repaint();
            };

            startTimerHz (10);
        }

        ~PropKnob() override   { stopTimer(); }

        void setArcColour (juce::Colour c)   { knob.setColour (juce::Slider::rotarySliderFillColourId, c); }

        void resized() override
        {
            auto r = getLocalBounds();
            r.removeFromBottom (28);
            const int size = juce::jmin (r.getWidth(), r.getHeight());
            knob.setBounds (r.withSizeKeepingCentre (size, size));
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            auto valueArea = r.removeFromBottom (13.0f);
            auto labelArea = r.removeFromBottom (15.0f);

            g.setFont (KngFonts::font (style.labelFace, 11.0f, 0.04f));
            g.setColour (style.label);
            g.drawText (label, labelArea, juce::Justification::centredBottom, false);

            g.setFont (KngFonts::font (style.valueFace, 11.0f));
            g.setColour (style.value);
            g.drawText (format (knob.getValue()), valueArea, juce::Justification::centred, false);
        }

    private:
        double read() const   { return (double) plugin.state.getProperty (id, fallback); }

        // Follow undo, presets and the Rack
        void timerCallback() override
        {
            const double v = read();

            if (! knob.isMouseButtonDown() && std::abs (v - knob.getValue()) > 1.0e-6)
            {
                knob.setValue (v, juce::dontSendNotification);
                repaint();
            }
        }

        KngSamplerPlugin& plugin;
        te::Plugin::Ptr pluginRef;
        const juce::Identifier id;
        const juce::String label;
        const double fallback;
        const bool whole;
        std::function<juce::String (double)> format;
        const KngFaceStyle style;
        juce::Slider knob;
    };

    /** An on/off pill bound to one of the Sampler's settings. */
    class PropPill : public juce::Component,
                     public juce::SettableTooltipClient,
                     private juce::Timer
    {
    public:
        PropPill (KngSamplerPlugin& p, const juce::Identifier& idToUse, const juce::String& labelText, const KngFaceStyle& s)
            : plugin (p), pluginRef (&p), id (idToUse), label (labelText), style (s)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            startTimerHz (8);
        }

        ~PropPill() override   { stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            const bool on = (bool) plugin.state.getProperty (id, false);
            const auto r = getLocalBounds().toFloat().reduced (1.0f);

            g.setColour (on ? violet : style.panel);
            g.fillRoundedRectangle (r, 10.0f);

            if (! on)
            {
                g.setColour (style.outline);
                g.drawRoundedRectangle (r, 10.0f, 1.0f);
            }

            const auto dot = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ r.getX() + 16.0f, r.getCentreY() });

            if (on)
            {
                g.setColour (ink);
                g.fillEllipse (dot);
            }
            else
            {
                g.setColour (muted);
                g.drawEllipse (dot, 1.0f);
            }

            g.setFont (KngFonts::font (KngFonts::sora, 12.0f));
            g.setColour (on ? ink : style.label);
            g.drawText (label, r.withTrimmedLeft (28.0f), juce::Justification::centredLeft, false);
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            if (auto* um = plugin.getUndoManager())
                um->beginNewTransaction();

            plugin.state.setProperty (id, ! (bool) plugin.state.getProperty (id, false), plugin.getUndoManager());
            plugin.syncFromState();
            repaint();
        }

    private:
        void timerCallback() override   { repaint(); }

        KngSamplerPlugin& plugin;
        te::Plugin::Ptr pluginRef;
        const juce::Identifier id;
        const juce::String label;
        const KngFaceStyle style;
    };

    juce::String msText (double v)
    {
        return v >= 1000.0 ? juce::String (v / 1000.0, 2) + " s" : juce::String (v, v < 10.0 ? 1 : 0) + " ms";
    }
}

//==============================================================================
KngSamplerKeys::KngSamplerKeys (juce::MidiKeyboardState& state, KngSamplerPlugin& owner, std::function<void (int)> onRootPicked)
    : juce::MidiKeyboardComponent (state, juce::MidiKeyboardComponent::horizontalKeyboard),
      plugin (owner), onRoot (std::move (onRootPicked))
{
    setTooltip ("Play the sample. Right-click a key to make it the root.");
}

void KngSamplerKeys::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                                    juce::Colour, juce::Colour)
{
    const int root = (int) plugin.state.getProperty (KngSamplerPlugin::rootProp, 60);
    const bool playing = isDown || note == plugin.displayNote.load();
    auto c = playing ? violet : note == root ? rootPink : juce::Colour (0xffe8e3f5);

    if (isOver && ! playing)
        c = c.interpolatedWith (violet, 0.25f);

    const auto key = area.reduced (1.0f, 0.0f);
    g.setColour (c);
    g.fillRoundedRectangle (key, 3.0f);

    if (note == root && key.getWidth() > 12.0f)
    {
        g.setFont (KngFonts::font (KngFonts::mono, juce::jmin (10.0f, key.getWidth() * 0.45f)));
        g.setColour (ink);
        g.drawText (Theory::noteName (root), key.removeFromBottom (16.0f), juce::Justification::centred, false);
    }
}

void KngSamplerKeys::drawBlackNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                                    juce::Colour)
{
    const int root = (int) plugin.state.getProperty (KngSamplerPlugin::rootProp, 60);
    const bool playing = isDown || note == plugin.displayNote.load();
    auto c = playing ? violet : note == root ? rootPink : juce::Colour (0xff1d1a26);

    if (isOver && ! playing)
        c = c.interpolatedWith (violet, 0.3f);

    g.setColour (c);
    g.fillRoundedRectangle (area.reduced (1.0f, 0.0f), 2.0f);
}

void KngSamplerKeys::mouseDown (const juce::MouseEvent& e)
{
    // Right-click picks the root instead of playing
    if (e.mods.isPopupMenu())
    {
        const auto hit = getNoteAndVelocityAtPosition (e.position);

        if (hit.note >= 0 && onRoot)
            onRoot (hit.note);

        repaint();
        return;
    }

    juce::MidiKeyboardComponent::mouseDown (e);
}

//==============================================================================
KngSamplerView::KngSamplerView (KngSamplerPlugin& owner, EditSession* sessionToUse)
    : plugin (owner), pluginRef (&owner), session (sessionToUse),
      style (makeSamplerStyle()), look (style),
      keyboard (keyboardState, owner, [this] (int note) { setProp (KngSamplerPlugin::rootProp, note); })
{
    setLookAndFeel (&look);

    addAndMakeVisible (loadButton);
    loadButton.setTooltip ("Load a sample from disk, or drag one onto the waveform");
    loadButton.onClick = [this] { chooseFile(); };

    addAndMakeVisible (popButton);
    popButton.setTooltip ("Open in its own window");
    popButton.onClick = [this] { KngPopOut::open (plugin); };

    // Knobs
    auto addKnob = [this] (const juce::Identifier& id, const char* label, double min, double max, double mid,
                           double step, double def, std::function<juce::String (double)> fmt, juce::Colour arc)
    {
        auto k = std::make_unique<PropKnob> (plugin, id, label, min, max, mid, step, def, std::move (fmt), style);
        k->setArcColour (arc);
        addAndMakeVisible (*k);
        knobs.push_back (std::move (k));
    };

    addKnob (KngSamplerPlugin::gainProp, "Gain", -24.0, 12.0, 0.0, 0.1, 0.0,
             [] (double v) { return (v > 0.0 ? "+" : "") + juce::String (v, 1) + " dB"; }, violet);
    addKnob (KngSamplerPlugin::attackProp, "Attack", 0.0, 500.0, 40.0, 0.1, 2.0, msText, violet);
    addKnob (KngSamplerPlugin::releaseProp, "Release", 1.0, 2000.0, 150.0, 0.1, 30.0, msText, violet);
    addKnob (KngSamplerPlugin::glideProp, "Glide", 0.0, 500.0, 60.0, 0.1, 0.0, msText, violet);
    addKnob (KngSamplerPlugin::rootProp, "Root", 24.0, 96.0, 0.0, 1.0, 60.0,
             [] (double v) { return Theory::noteName (juce::roundToInt (v)); }, rootPink);

    // Switches
    auto addPill = [this] (const juce::Identifier& id, const char* label, const char* tip)
    {
        auto p = std::make_unique<PropPill> (plugin, id, label, style);
        p->setTooltip (tip);
        addAndMakeVisible (*p);
        pills.push_back (std::move (p));
    };

    addPill (KngSamplerPlugin::monoProp, "Mono", "One note at a time; each new note cuts the last. Turn on for 808s.");
    addPill (KngSamplerPlugin::oneShotProp, "One-shot", "Always play the whole sample, ignoring note length");
    addPill (KngSamplerPlugin::loopProp, "Loop", "Hold the sound for long notes by looping the purple region");

    addAndMakeVisible (keyboard);
    keyboard.setAvailableRange (24, 96);
    keyboard.setLowestVisibleKey (36);
    keyboardState.addListener (this);

    setSize (1000, 560);
    startTimerHz (30);
}

KngSamplerView::~KngSamplerView()
{
    stopTimer();
    keyboardState.removeListener (this);
    setLookAndFeel (nullptr);
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
void KngSamplerView::resized()
{
    panel = getLocalBounds().reduced (8);
    auto inner = panel.reduced (20, 14);

    header = inner.removeFromTop (40);
    {
        auto h = header;
        popButton.setBounds (h.removeFromRight (32).withSizeKeepingCentre (32, 32));
        h.removeFromRight (10);
        loadButton.setBounds (h.removeFromRight (120).withSizeKeepingCentre (120, 32));
    }

    inner.removeFromTop (12);

    auto controls = inner.removeFromBottom (juce::jlimit (78, 100, inner.getHeight() / 3));
    inner.removeFromBottom (10);

    auto keys = inner.removeFromBottom (juce::jlimit (30, 56, inner.getHeight() / 4));
    keyboard.setBounds (keys);
    keyboard.setKeyWidth (juce::jmax (10.0f, (float) keys.getWidth() / 21.0f));
    inner.removeFromBottom (10);

    wave = inner;

    const int k = juce::jlimit (36, 54, controls.getHeight() - 34);

    for (auto& knob : knobs)
        knob->setBounds (controls.removeFromLeft (k + 44).withSizeKeepingCentre (k + 40, k + 32));

    auto pillRow = controls.removeFromRight (juce::jmin (controls.getWidth(), 3 * 108));

    for (auto& pill : pills)
        pill->setBounds (pillRow.removeFromLeft (108).withSizeKeepingCentre (100, 32));
}

int KngSamplerView::fracToX (float f) const
{
    const auto w = wave.reduced (16, 0);
    return w.getX() + juce::roundToInt (f * w.getWidth());
}

float KngSamplerView::xToFrac (int x) const
{
    const auto w = wave.reduced (16, 0);
    return juce::jlimit (0.0f, 1.0f, (x - w.getX()) / (float) juce::jmax (1, w.getWidth()));
}

//==============================================================================
void KngSamplerView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0b10));

    const auto p = panel.toFloat();
    juce::ColourGradient bg (juce::Colour (0xff1c1924), p.getX(), p.getY(), juce::Colour (0xff0f0d14), p.getRight(), p.getBottom(), false);
    bg.addColour (0.6, juce::Colour (0xff121018));
    g.setGradientFill (bg);
    g.fillRoundedRectangle (p, 18.0f);
    g.setColour (juce::Colour (0xff2a2635));
    g.drawRoundedRectangle (p.reduced (0.5f), 18.0f, 1.0f);

    paintHeader (g);
    paintWave (g);
}

void KngSamplerView::paintHeader (juce::Graphics& g)
{
    const auto h = header.toFloat();
    const float right = (float) loadButton.getX() - 12.0f;

    g.setFont (KngFonts::font (KngFonts::sora, 24.0f));
    g.setColour (paper);
    g.drawText ("KNG", juce::Rectangle<float> (h.getX(), h.getY(), 60.0f, h.getHeight()), juce::Justification::centredLeft, false);
    g.setColour (violet);
    g.drawText ("Sampler", juce::Rectangle<float> (h.getX() + 58.0f, h.getY(), 120.0f, h.getHeight()), juce::Justification::centredLeft, false);

    float x = h.getX() + 186.0f;

    if (x + 80.0f > right)
        return;

    // The sample's name in a pill
    auto name = plugin.getSampleFile().getFileName();

    if (name.isEmpty())
        name = "No sample yet";

    g.setFont (KngFonts::font (KngFonts::mono, 12.0f));
    const float pillW = juce::jmin (300.0f, right - x, 40.0f + (float) name.length() * 7.5f);
    const auto pill = juce::Rectangle<float> (x, h.getCentreY() - 15.0f, pillW, 30.0f);

    g.setColour (juce::Colour (0xff0b0a0f));
    g.fillRoundedRectangle (pill, 8.0f);
    g.setColour (juce::Colour (0xff2a2635));
    g.drawRoundedRectangle (pill.reduced (0.5f), 8.0f, 1.0f);
    g.setColour (violet);
    g.fillEllipse (pill.getX() + 12.0f, pill.getCentreY() - 4.0f, 8.0f, 8.0f);
    g.setColour (juce::Colour (0xffd9ceff));
    g.drawText (name, pill.withTrimmedLeft (28.0f).withTrimmedRight (10.0f), juce::Justification::centredLeft, true);

    x = pill.getRight() + 12.0f;

    // Sample rate, length, channels
    if (auto buffer = plugin.getSampleForDisplay())
    {
        const double rate = juce::jmax (1.0, plugin.getFileSampleRate());
        const juce::String dot (juce::CharPointer_UTF8 (" \xc2\xb7 "));
        const auto info = juce::String (rate / 1000.0, 1) + " kHz" + dot
                        + juce::String (buffer->getNumSamples() / rate, 2) + " s" + dot
                        + (buffer->getNumChannels() > 1 ? "stereo" : "mono");

        if (right - x > 60.0f)
        {
            g.setFont (KngFonts::font (KngFonts::mono, 11.0f));
            g.setColour (muted);
            g.drawText (info, juce::Rectangle<float> (x, h.getY(), right - x, h.getHeight()), juce::Justification::centredLeft, true);
        }
    }
}

void KngSamplerView::paintWave (juce::Graphics& g)
{
    if (wave.getHeight() < 30)
        return;

    const auto w = wave.toFloat();
    g.setColour (hovering ? violet.withAlpha (0.12f) : juce::Colour (0xff0b0a0f));
    g.fillRoundedRectangle (w, 14.0f);
    g.setColour (hovering ? violet : juce::Colour (0xff241f30));
    g.drawRoundedRectangle (w.reduced (hovering ? 1.0f : 0.5f), 14.0f, hovering ? 2.0f : 1.0f);

    if (peaks.empty())
    {
        g.setFont (KngFonts::font (KngFonts::sora, 15.0f));
        g.setColour (muted);
        g.drawText ("Drop a sample here, or click Load sample", w, juce::Justification::centred, false);
        return;
    }

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (wave.reduced (1));

    const bool loopOn = (bool) plugin.state.getProperty (KngSamplerPlugin::loopProp, false);
    const float ls = propFloat (KngSamplerPlugin::loopStartProp, 0.4f);
    const float le = propFloat (KngSamplerPlugin::loopEndProp, 0.9f);

    // Loop region behind the bars
    if (loopOn)
    {
        const float x1 = (float) fracToX (ls), x2 = (float) fracToX (le);
        juce::ColourGradient region (violet.withAlpha (0.16f), 0.0f, w.getY(), violet.withAlpha (0.06f), 0.0f, w.getBottom(), false);
        g.setGradientFill (region);
        g.fillRect (x1, w.getY(), x2 - x1, w.getHeight());

        g.setColour (violet);
        g.fillRect (x1 - 1.0f, w.getY(), 2.0f, w.getHeight());
        g.fillRect (x2 - 1.0f, w.getY(), 2.0f, w.getHeight());

        g.setFont (KngFonts::font (KngFonts::mono, 10.0f, 0.1f));
        g.setColour (juce::Colour (0xffc4b2ff));
        g.drawText ("LOOP START", juce::Rectangle<float> (x1 + 8.0f, w.getY() + 8.0f, 90.0f, 14.0f), juce::Justification::centredLeft, false);
        g.drawText ("LOOP END", juce::Rectangle<float> (x2 + 8.0f, w.getY() + 8.0f, 80.0f, 14.0f), juce::Justification::centredLeft, false);
    }

    // The bars
    const auto area = w.reduced (16.0f, 28.0f);
    const int n = (int) peaks.size();
    const float step = area.getWidth() / (float) juce::jmax (1, n);

    for (int i = 0; i < n; ++i)
    {
        const float frac = (float) i / (float) n;
        const float hgt = juce::jmax (3.0f, peaks[(size_t) i] * area.getHeight());
        const bool inLoop = loopOn && frac >= ls && frac <= le;

        g.setColour (inLoop ? lilac : deepViolet);
        g.fillRoundedRectangle (area.getX() + i * step, area.getCentreY() - hgt * 0.5f, juce::jmax (1.5f, step - 1.0f), hgt, 1.5f);
    }

    // Playhead
    const float pos = plugin.displayPos.load();

    if (pos >= 0.0f && pos <= 1.0f)
    {
        const float px = (float) fracToX (pos);
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.fillRect (px - 3.0f, w.getY(), 6.0f, w.getHeight());
        g.setColour (juce::Colours::white);
        g.fillRect (px - 1.0f, w.getY(), 2.0f, w.getHeight());
    }

    if (loopOn && w.getHeight() > 120.0f)
    {
        g.setFont (KngFonts::font (KngFonts::mono, 10.0f));
        g.setColour (juce::Colour (0xff5d5672));
        g.drawText ("Drag the purple edges to set the loop", w.reduced (16.0f, 8.0f).removeFromBottom (14.0f),
                    juce::Justification::centredLeft, false);
    }
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
