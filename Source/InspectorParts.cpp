#include "InspectorParts.h"
#include "GenericPluginPanel.h"

namespace te = tracktion;

//==============================================================================
ParamKnob::ParamKnob (EditSession& sessionToUse, te::AutomatableParameter::Ptr paramToUse)
    : session (sessionToUse), param (paramToUse)
{
    addAndMakeVisible (knob);
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

    const auto range = param->getValueRange();
    knob.setRange (range.getStart(), range.getEnd(), 0.0);
    knob.setValue (param->getCurrentValue(), juce::dontSendNotification);
    knob.setPopupDisplayEnabled (true, true, nullptr);
    knob.setTooltip (param->getParameterName());

    auto p = param;
    knob.textFromValueFunction = [p] (double v) { return p->valueToString ((float) v); };

    knob.onDragStart   = [this] { session.beginEdit(); };
    knob.onValueChange = [this] { param->setParameter ((float) knob.getValue(), juce::sendNotification); };
    knob.onDragEnd     = [this] { session.save(); };

    addAndMakeVisible (label);
    label.setText (param->getParameterName(), juce::dontSendNotification);
    label.setFont (juce::Font (juce::FontOptions (11.0f)));
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    label.setInterceptsMouseClicks (false, false);
    label.setMinimumHorizontalScale (0.7f);

    startTimerHz (10);
}

ParamKnob::~ParamKnob()
{
    stopTimer();
}

void ParamKnob::timerCallback()
{
    if (! knob.isMouseButtonDown())
        knob.setValue (param->getCurrentValue(), juce::dontSendNotification);
}

void ParamKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromBottom (14));
    const int size = juce::jmin (r.getWidth(), r.getHeight());
    knob.setBounds (r.withSizeKeepingCentre (size, size));
}

//==============================================================================
EffectRow::EffectRow (EditSession& sessionToUse, te::Plugin& pluginToUse)
    : session (sessionToUse), plugin (&pluginToUse)
{
    setTooltip ("Click to open. Click the dot to turn it on or off. Right-click for more.");
    startTimerHz (4);
}

EffectRow::~EffectRow()
{
    stopTimer();
}

void EffectRow::openPlugin (EditSession& session, te::Plugin& p, juce::Component& anchor)
{
    if (dynamic_cast<te::ExternalPlugin*> (&p) != nullptr)
        p.showWindowExplicitly();
    else if (p.getPluginType().startsWith ("kng") && session.onShowPluginEditor)
        session.onShowPluginEditor (p);
    else
        GenericPluginPanel::showFor (p, anchor);
}

void EffectRow::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff2c3137));
    g.fillRoundedRectangle (r, 6.0f);

    const bool on = plugin->isEnabled();
    g.setColour (on ? juce::Colour (0xff5ad45a) : juce::Colours::white.withAlpha (0.25f));
    g.fillEllipse (12.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);

    auto text = getLocalBounds().withTrimmedLeft (28).withTrimmedRight (10);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawText ("Open", text.removeFromRight (44), juce::Justification::centredRight);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.92f : 0.45f));
    g.drawText (plugin->getName(), text, juce::Justification::centredLeft, true);
}

void EffectRow::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
    {
        // The dot turns it on or off; anywhere else opens it
        if (e.x < 28)
            plugin->setEnabled (! plugin->isEnabled());
        else
            openPlugin (session, *plugin, *this);

        repaint();
        return;
    }

    auto* s = &session;
    auto ptr = plugin;
    juce::Component::SafePointer<EffectRow> safe (this);
    juce::PopupMenu menu;

    menu.addItem ("Open", [s, ptr, safe] { if (safe != nullptr) openPlugin (*s, *ptr, *safe); });
    menu.addItem (ptr->isEnabled() ? "Bypass" : "Turn On", [ptr] { ptr->setEnabled (! ptr->isEnabled()); });

    if (auto* list = ptr->getOwnerList())
    {
        menu.addItem ("Move Up", [s, list, ptr] { s->movePlugin (*list, ptr.get(), -1); });
        menu.addItem ("Move Down", [s, list, ptr] { s->movePlugin (*list, ptr.get(), 1); });
    }

    menu.addSeparator();
    menu.addItem ("Remove", [s, ptr] { juce::MessageManager::callAsync ([s, ptr] { s->removePlugin (ptr.get()); }); });
    menu.showMenuAsync ({});
}

//==============================================================================
juce::Array<te::AutomatableParameter*> pickMainParams (te::Plugin& plugin, int maxCount)
{
    auto all = plugin.getAutomatableParameters();
    juce::Array<te::AutomatableParameter*> picked;

    for (auto* want : { "cutoff", "reso", "attack", "release" })
    {
        for (auto* p : all)
        {
            if (picked.size() < maxCount && ! picked.contains (p) && p->getParameterName().containsIgnoreCase (want))
            {
                picked.add (p);
                break;
            }
        }
    }

    for (auto* p : all)
        if (picked.size() < maxCount && ! picked.contains (p))
            picked.add (p);

    return picked;
}
