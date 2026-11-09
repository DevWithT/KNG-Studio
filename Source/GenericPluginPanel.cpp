#include "GenericPluginPanel.h"

namespace te = tracktion;

GenericPluginPanel::GenericPluginPanel (te::Plugin& p)
    : plugin (&p)
{
    addAndMakeVisible (titleLabel);
    titleLabel.setText (p.getName(), juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));

    addAndMakeVisible (enabledButton);
    enabledButton.setToggleState (p.isEnabled(), juce::dontSendNotification);
    enabledButton.setTooltip ("Turn this effect on or off");
    enabledButton.onClick = [this] { plugin->setEnabled (enabledButton.getToggleState()); };

    for (auto* param : p.getAutomatableParameters())
    {
        Row row;
        row.param = param;

        row.name = std::make_unique<juce::Label> (juce::String(), param->getParameterName());
        row.value = std::make_unique<juce::Label>();
        row.value->setJustificationType (juce::Justification::centredRight);
        row.value->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));

        row.slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        const auto& r = param->valueRange;
        row.slider->setNormalisableRange ({ (double) r.start, (double) r.end, (double) r.interval, (double) r.skew });
        row.slider->setValue (param->getCurrentValue(), juce::dontSendNotification);

        auto* slider = row.slider.get();
        row.slider->onValueChange = [param, slider] { param->setParameter ((float) slider->getValue(), juce::sendNotification); };

        addAndMakeVisible (*row.name);
        addAndMakeVisible (*row.slider);
        addAndMakeVisible (*row.value);
        rows.push_back (std::move (row));
    }

    setSize (480, 56 + (int) rows.size() * 30);
    startTimerHz (10);
}

GenericPluginPanel::~GenericPluginPanel()
{
    stopTimer();
}

void GenericPluginPanel::showFor (te::Plugin& plugin, juce::Component& anchor)
{
    juce::CallOutBox::launchAsynchronously (std::make_unique<GenericPluginPanel> (plugin),
                                            anchor.getScreenBounds(), nullptr);
}

void GenericPluginPanel::timerCallback()
{
    // Follow automation and undo
    for (auto& row : rows)
    {
        if (! row.slider->isMouseButtonDown())
            row.slider->setValue (row.param->getCurrentValue(), juce::dontSendNotification);

        row.value->setText (row.param->getCurrentValueAsString(), juce::dontSendNotification);
    }

    enabledButton.setToggleState (plugin->isEnabled(), juce::dontSendNotification);
}

void GenericPluginPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2124));
}

void GenericPluginPanel::resized()
{
    auto r = getLocalBounds().reduced (12);

    auto top = r.removeFromTop (28);
    enabledButton.setBounds (top.removeFromRight (60));
    titleLabel.setBounds (top);
    r.removeFromTop (8);

    for (auto& row : rows)
    {
        auto line = r.removeFromTop (26);
        row.name->setBounds (line.removeFromLeft (140));
        row.value->setBounds (line.removeFromRight (90));
        row.slider->setBounds (line.reduced (4, 0));
        r.removeFromTop (4);
    }
}
