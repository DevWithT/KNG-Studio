#include "KngFaceView.h"
#include "KngPopOut.h"

namespace te = tracktion;

KngFaceStyle kngMakeStyle (juce::uint32 accent, juce::uint32 bodyLight, juce::uint32 bodyDark, juce::uint32 pointer,
                           juce::uint32 label, juce::uint32 value, juce::uint32 panel, juce::uint32 outline,
                           KngFonts::Face labelFace, bool upperCaseLabels)
{
    KngFaceStyle s;
    s.accent    = juce::Colour (accent);
    s.bodyLight = juce::Colour (bodyLight);
    s.bodyDark  = juce::Colour (bodyDark);
    s.pointer   = juce::Colour (pointer);
    s.track     = juce::Colours::white.withAlpha (0.07f);
    s.label     = juce::Colour (label);
    s.value     = juce::Colour (value);
    s.panel     = juce::Colour (panel);
    s.outline   = juce::Colour (outline);
    s.labelFace = labelFace;
    s.valueFace = KngFonts::mono;
    s.menuFace  = KngFonts::mono;
    s.upperCaseLabels = upperCaseLabels;
    return s;
}

namespace
{
    /** An on/off pill for the header, bound to one parameter. */
    class HeaderSwitch : public juce::Component,
                         public juce::SettableTooltipClient,
                         private juce::Timer
    {
    public:
        HeaderSwitch (te::Plugin& p, te::AutomatableParameter& parameter, const juce::String& text, const KngFaceStyle& s)
            : pluginRef (&p), param (&parameter), label (text), style (s)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            startTimerHz (8);
        }

        ~HeaderSwitch() override   { stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            const bool on = param->getCurrentValue() > 0.5f;
            auto r = getLocalBounds().toFloat();
            const auto pill = r.removeFromLeft (40.0f).withSizeKeepingCentre (40.0f, 22.0f);

            if (on)
            {
                g.setColour (style.accent.withAlpha (0.3f));
                g.fillRoundedRectangle (pill.expanded (3.0f), 14.0f);
            }

            g.setColour (on ? style.accent : style.panel);
            g.fillRoundedRectangle (pill, 11.0f);
            g.setColour (style.outline);
            g.drawRoundedRectangle (pill.reduced (0.5f), 11.0f, 1.0f);

            const float dot = on ? pill.getRight() - 19.0f : pill.getX() + 3.0f;
            g.setColour (on ? juce::Colours::white : style.label);
            g.fillEllipse (dot, pill.getY() + 3.0f, 16.0f, 16.0f);

            g.setFont (KngFonts::font (style.labelFace, 13.0f, 0.08f));
            g.setColour (on ? juce::Colours::white.withAlpha (0.92f) : style.label);
            g.drawText (style.upperCaseLabels ? label.toUpperCase() : label, r.withTrimmedLeft (8.0f),
                        juce::Justification::centredLeft, false);
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            if (auto* um = pluginRef->getUndoManager())
                um->beginNewTransaction();

            param->setParameter (param->getCurrentValue() > 0.5f ? 0.0f : 1.0f, juce::sendNotification);
            repaint();
        }

    private:
        void timerCallback() override   { repaint(); }

        te::Plugin::Ptr pluginRef;
        te::AutomatableParameter::Ptr param;
        juce::String label;
        const KngFaceStyle style;
    };
}

//==============================================================================
KngFaceView::KngFaceView (te::Plugin& pluginToShow, const KngFaceStyle& styleToUse, Palette paletteToUse)
    : plugin (pluginToShow), style (styleToUse), palette (paletteToUse), look (style), pluginRef (&pluginToShow)
{
    setLookAndFeel (&look);

    addAndMakeVisible (presetBox);
    presetBox.setTooltip ("Factory presets, your own presets, and Save Preset");

    addAndMakeVisible (popButton);
    popButton.setTooltip ("Open in its own window");
    popButton.onClick = [this] { KngPopOut::open (plugin); };

    setSize (1000, 520);
    startTimerHz (30);
}

KngFaceView::~KngFaceView()
{
    stopTimer();
    knobs.clear();
    headerItems.clear();
    setLookAndFeel (nullptr);
}

void KngFaceView::attachPresets (const juce::StringArray& names, std::function<void (int)> apply)
{
    presetBox.attach (plugin, names, std::move (apply));
}

KngFaceKnob& KngFaceView::addKnob (te::AutomatableParameter& param, const KngParamDef& def, const juce::String& label)
{
    auto k = std::make_unique<KngFaceKnob> (plugin, param, def, style, label);
    auto& ref = *k;
    addAndMakeVisible (ref);
    knobs.push_back (std::move (k));
    return ref;
}

KngFaceKnob& KngFaceView::addKnob (KngFxBase& fx, int index, const juce::String& label)
{
    auto k = std::make_unique<KngFaceKnob> (fx, index, style, label);
    auto& ref = *k;
    addAndMakeVisible (ref);
    knobs.push_back (std::move (k));
    return ref;
}

void KngFaceView::addHeaderChoice (te::AutomatableParameter& param, float minValue, const juce::String& names, int width)
{
    const int count = juce::jmax (1, juce::StringArray::fromTokens (names, "|", "").size());
    const KngParamDef def { "", "", minValue, minValue + (float) (count - 1), minValue, 0.0f, 1.0f, "", nullptr };

    auto c = std::make_unique<KngFaceChoice> (plugin, param, def, style, names);
    addAndMakeVisible (*c);
    headerItems.push_back (std::move (c));
    headerWidths.push_back (width);
}

void KngFaceView::addHeaderSwitch (te::AutomatableParameter& param, const juce::String& label, const juce::String& tip, int width)
{
    auto s = std::make_unique<HeaderSwitch> (plugin, param, label, style);
    s->setTooltip (tip);
    addAndMakeVisible (*s);
    headerItems.push_back (std::move (s));
    headerWidths.push_back (width);
}

void KngFaceView::clearKnobs()
{
    knobs.clear();
    lastVisible.clear();
}

//==============================================================================
void KngFaceView::timerCallback()
{
    popButton.setVisible (! KngPopOut::isPoppedOut (*this));
    tick();

    // A face can show and hide knobs (Sync on the Delay, for example): lay them out again when it does
    juce::String visible;

    for (auto& k : knobs)
        visible << (k->isVisible() ? "1" : "0");

    if (visible != lastVisible)
    {
        lastVisible = visible;
        resized();
    }

    repaint (displayArea.toNearestInt().expanded (2));

    if (! sideArea.isEmpty())
        repaint (sideArea.toNearestInt().expanded (2));
}

void KngFaceView::resized()
{
    panel = getLocalBounds().reduced (8);
    auto inner = panel.reduced (22, 14);

    header = inner.removeFromTop (40);
    {
        auto h = header;
        popButton.setBounds (h.removeFromRight (32).withSizeKeepingCentre (32, 32));
        h.removeFromRight (10);
        presetBox.setBounds (h.removeFromRight (190).withSizeKeepingCentre (190, 32));
        h.removeFromRight (12);
        titleArea = h.removeFromLeft (juce::jmin (titleWidth, h.getWidth()));

        for (size_t i = 0; i < headerItems.size(); ++i)
        {
            h.removeFromLeft (10);
            const int w = juce::jmin (headerWidths[i], h.getWidth());
            headerItems[i]->setVisible (w > 40);
            headerItems[i]->setBounds (h.removeFromLeft (w).withSizeKeepingCentre (w, 26));
        }
    }

    inner.removeFromTop (10);
    auto row = inner.removeFromBottom (juce::jlimit (84, 108, inner.getHeight() / 3));
    inner.removeFromBottom (10);

    sideArea = {};

    if (sideWidth > 0)
    {
        sideArea = inner.removeFromRight (sideWidth).toFloat();
        inner.removeFromRight (10);
    }

    displayArea = inner.toFloat();

    std::vector<juce::Component*> shown;

    for (auto& k : knobs)
        if (k->isVisible())
            shown.push_back (k.get());

    if (! shown.empty())
    {
        const int colW = juce::jmin (maxKnobWidth, row.getWidth() / (int) shown.size());
        auto r = row.withSizeKeepingCentre (colW * (int) shown.size(), row.getHeight());

        for (auto* k : shown)
            k->setBounds (r.removeFromLeft (colW).reduced (2, 0));
    }
}

void KngFaceView::paint (juce::Graphics& g)
{
    g.fillAll (palette.page);

    const auto p = panel.toFloat();
    juce::ColourGradient bg (palette.top, p.getX(), p.getY(), palette.bottom, p.getRight(), p.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (p, 18.0f);
    g.setColour (palette.edge);
    g.drawRoundedRectangle (p.reduced (0.5f), 18.0f, 1.0f);

    paintTitle (g, titleArea.toFloat());

    if (displayArea.getWidth() > 40.0f && displayArea.getHeight() > 30.0f)
        paintDisplay (g, displayArea);

    if (! sideArea.isEmpty())
        paintSide (g, sideArea);
}

//==============================================================================
void KngFaceView::drawTitle (juce::Graphics& g, juce::Rectangle<float> a, const juce::String& word,
                             KngFonts::Face face, float size) const
{
    g.setFont (KngFonts::font (style.labelFace, 13.0f, 0.3f));
    g.setColour (style.label);
    g.drawText ("KNG", a.removeFromLeft (46.0f), juce::Justification::centredLeft, false);

    g.setFont (KngFonts::font (face, size));
    g.setColour (style.accent.withAlpha (0.18f));

    for (auto offset : { juce::Point<float> (-2.0f, 0.0f), { 2.0f, 0.0f }, { 0.0f, -2.0f }, { 0.0f, 2.0f } })
        g.drawText (word, a.translated (offset.x, offset.y), juce::Justification::centredLeft, false);

    g.setColour (style.accent);
    g.drawText (word, a, juce::Justification::centredLeft, false);
}

void KngFaceView::drawScreen (juce::Graphics& g, juce::Rectangle<float> area) const
{
    g.setColour (juce::Colours::black.withAlpha (0.42f));
    g.fillRoundedRectangle (area, 12.0f);
    g.setColour (style.accent.withAlpha (0.18f));
    g.drawRoundedRectangle (area.reduced (0.5f), 12.0f, 1.0f);
}

void KngFaceView::drawMeter (juce::Graphics& g, juce::Rectangle<float> area, float db, const juce::String& label) const
{
    auto labelArea = area.removeFromBottom (16.0f);

    g.setColour (juce::Colours::black.withAlpha (0.42f));
    g.fillRoundedRectangle (area, 4.0f);

    const float level = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    auto bar = area.reduced (3.0f);
    g.setColour (db > -0.5f ? juce::Colour (0xffff4d4d) : style.accent);
    g.fillRoundedRectangle (bar.removeFromBottom (bar.getHeight() * level), 2.0f);

    g.setFont (KngFonts::font (KngFonts::mono, 10.0f));
    g.setColour (style.label);
    g.drawText (label, labelArea, juce::Justification::centred, false);
}

void KngFaceView::drawInOut (juce::Graphics& g, juce::Rectangle<float> area, float inDb, float outDb) const
{
    const float w = (area.getWidth() - 8.0f) * 0.5f;
    drawMeter (g, area.removeFromLeft (w), inDb, "IN");
    area.removeFromLeft (8.0f);
    drawMeter (g, area, outDb, "OUT");
}

float KngFaceView::fallingDb (float gain, float previousDb)
{
    const float db = juce::Decibels::gainToDecibels (gain, -100.0f);
    return db > previousDb ? db : previousDb - 1.2f;
}
