#include "KngSamplerView.h"
#include "EditSession.h"
#include "TheoryHelper.h"
#include "KitBrowser.h"
#include "KngPopOut.h"

namespace te = tracktion;

static const juce::Colour violet (0xff9b7bff), lilac (0xffb9a1ff), deepViolet (0xff7a62d6), rootPink (0xffff9bd2),
                          paper (0xfff1ecff), muted (0xff6d6684), ink (0xff130c2b);

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
        g.drawText (Theory::noteName (root), key.withTrimmedTop (key.getHeight() - 16.0f), juce::Justification::centred, false);
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
