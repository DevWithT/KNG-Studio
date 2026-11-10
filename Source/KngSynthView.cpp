#include "KngSynthView.h"
#include "KngSynthPlugin.h"
#include "KngSynthPresets.h"

namespace te = tracktion;

static juce::String formatValue (int p, double v)
{
    using namespace KngSynth;

    switch (p)
    {
        case cutoff:
            return v >= 1000.0 ? juce::String (v / 1000.0, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";

        case lfoRate:
            return juce::String (v, 2) + " Hz";

        case master:
            return juce::String (v, 1) + " dB";

        case oscAFine: case oscBFine:
            return juce::String (juce::roundToInt (v)) + " ct";

        case fenvA: case fenvD: case fenvR: case ampA: case ampD: case ampR: case glide:
            return v < 1.0 ? juce::String (juce::roundToInt (v * 1000.0)) + " ms" : juce::String (v, 2) + " s";

        case oscAOct: case oscBOct: case oscASemi: case oscBSemi: case oscAUnison: case oscBUnison:
            return juce::String (juce::roundToInt (v));

        default:
            return juce::String (juce::roundToInt (v * 100.0)) + "%";
    }
}

//==============================================================================
KngSynthView::KngSynthView (EditSession& sessionToUse, te::Track& trackToUse)
    : session (sessionToUse), track (&trackToUse)
{
    plugin = trackToUse.pluginList.findFirstPluginOfType<KngSynthPlugin>();
    pluginRef = plugin;

    addAndMakeVisible (titleLabel);
    titleLabel.setText ("KNG Synth", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)));

    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.setTooltip ("Load a factory sound, then tweak it");

    const auto& list = KngSynth::presets();

    for (int i = 0; i < (int) list.size(); ++i)
        presetBox.addItem (list[(size_t) i].name, i + 1);

    presetBox.onChange = [this]
    {
        if (plugin == nullptr || presetBox.getSelectedId() <= 0)
            return;

        session.beginEdit();
        plugin->applyPreset (presetBox.getSelectedId() - 1);
        session.save();
    };

    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);

    content.onPaint = [this] (juce::Graphics& g)
    {
        for (auto& s : sections)
        {
            g.setColour (juce::Colour (0xff24282c));
            g.fillRoundedRectangle (s.bounds.toFloat(), 6.0f);

            g.setColour (juce::Colour (0xff3a8ee6));
            g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
            g.drawText (s.name.toUpperCase(), s.bounds.withHeight (20).reduced (10, 0), juce::Justification::centredLeft);
        }
    };

    if (plugin != nullptr)
        buildControls();

    addAndMakeVisible (keyboard);
    keyboard.setAvailableRange (24, 96);
    keyboard.setLowestVisibleKey (48);
    keyboardState.addListener (this);

    startTimerHz (10);
}

KngSynthView::~KngSynthView()
{
    stopTimer();
    keyboardState.removeListener (this);
    viewport.setViewedComponent (nullptr, false);
}

void KngSynthView::buildControls()
{
    for (auto& sec : KngSynth::sections())
    {
        SectionBox box;
        box.name = sec.name;

        for (int p : sec.params)
        {
            const auto& in = KngSynth::info (p);
            auto* param = plugin->getParam (p);

            Control c;
            c.param = p;

            c.label = std::make_unique<juce::Label> (juce::String(), in.name);
            c.label->setJustificationType (juce::Justification::centred);
            c.label->setFont (juce::Font (juce::FontOptions (11.0f)));
            c.label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
            content.addAndMakeVisible (*c.label);

            if (in.choices != nullptr)
            {
                c.box = std::make_unique<juce::ComboBox>();
                auto* combo = c.box.get();
                const auto names = juce::StringArray::fromTokens (in.choices, "|", "");

                for (int i = 0; i < names.size(); ++i)
                    combo->addItem (names[i], i + 1);

                const float minValue = in.min;
                combo->setSelectedId (juce::roundToInt (param->getCurrentValue() - minValue) + 1, juce::dontSendNotification);
                combo->onChange = [this, param, combo, minValue]
                {
                    session.beginEdit();
                    param->setParameter (minValue + (float) (combo->getSelectedId() - 1), juce::sendNotification);
                    session.save();
                };

                content.addAndMakeVisible (*combo);
            }
            else
            {
                c.knob = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
                auto* knob = c.knob.get();

                knob->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
                knob->setRange (in.min, in.max, in.step);

                if (in.mid > in.min && in.mid < in.max)
                    knob->setSkewFactorFromMidPoint (in.mid);

                knob->setDoubleClickReturnValue (true, in.def);
                knob->textFromValueFunction = [p] (double v) { return formatValue (p, v); };
                knob->setValue (param->getCurrentValue(), juce::dontSendNotification);

                knob->onDragStart   = [this] { session.beginEdit(); };
                knob->onValueChange = [param, knob] { param->setParameter ((float) knob->getValue(), juce::sendNotification); };
                knob->onDragEnd     = [this] { session.save(); };

                content.addAndMakeVisible (*knob);
            }

            box.controls.push_back ((int) controls.size());
            controls.push_back (std::move (c));
        }

        sections.push_back (box);
    }
}

void KngSynthView::layoutContent (int width)
{
    const int knobW = 64, comboW = 104, cellH = 84, pad = 10, titleH = 22, gap = 8;
    int x = 0, y = 0, rowH = 0;

    for (auto& s : sections)
    {
        int w = pad * 2;

        for (int ci : s.controls)
            w += controls[(size_t) ci].box != nullptr ? comboW : knobW;

        // Wrap to a new row when this section doesn't fit
        if (x > 0 && x + w > width)
        {
            x = 0;
            y += rowH + gap;
            rowH = 0;
        }

        s.bounds = { x, y, w, titleH + cellH + pad };
        int cx = x + pad;

        for (int ci : s.controls)
        {
            auto& c = controls[(size_t) ci];
            const int cw = c.box != nullptr ? comboW : knobW;
            juce::Rectangle<int> cell (cx, y + titleH, cw, cellH);

            c.label->setBounds (cell.removeFromTop (14));

            if (c.box != nullptr)
                c.box->setBounds (cell.withSizeKeepingCentre (cw - 8, 26));
            else
                c.knob->setBounds (cell);

            cx += cw;
        }

        x += w + gap;
        rowH = juce::jmax (rowH, s.bounds.getHeight());
    }

    content.setSize (juce::jmax (1, width), y + rowH + 4);
}

//==============================================================================
void KngSynthView::timerCallback()
{
    // Follow automation and undo
    for (auto& c : controls)
    {
        auto* param = plugin != nullptr ? plugin->getParam (c.param) : nullptr;

        if (param == nullptr)
            continue;

        const float v = param->getCurrentValue();

        if (c.knob != nullptr && ! c.knob->isMouseButtonDown())
            c.knob->setValue (v, juce::dontSendNotification);

        if (c.box != nullptr && ! c.box->isPopupActive())
            c.box->setSelectedId (juce::roundToInt (v - KngSynth::info (c.param).min) + 1, juce::dontSendNotification);
    }
}

void KngSynthView::handleNoteOn (juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity)
{
    if (auto* at = dynamic_cast<te::AudioTrack*> (track.get()))
        at->playGuideNote (midiNoteNumber, te::MidiChannel (1), juce::roundToInt (velocity * 127.0f), false, true, false);
}

void KngSynthView::handleNoteOff (juce::MidiKeyboardState*, int, int, float)
{
    if (auto* at = dynamic_cast<te::AudioTrack*> (track.get()))
        at->turnOffGuideNotes (te::MidiChannel (1));
}

//==============================================================================
void KngSynthView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d20));

    if (plugin == nullptr)
    {
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.drawText ("No KNG Synth on this track.", getLocalBounds(), juce::Justification::centred);
    }
}

void KngSynthView::resized()
{
    auto r = getLocalBounds().reduced (8);

    auto top = r.removeFromTop (28);
    titleLabel.setBounds (top.removeFromLeft (120));
    presetBox.setBounds (top.removeFromLeft (220));

    r.removeFromTop (6);
    keyboard.setBounds (r.removeFromBottom (56));
    r.removeFromBottom (6);

    viewport.setBounds (r);
    layoutContent (r.getWidth() - viewport.getScrollBarThickness());
}
