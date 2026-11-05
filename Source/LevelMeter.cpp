#include "LevelMeter.h"

namespace te = tracktion;

LevelMeter::LevelMeter()
{
    startTimerHz (30);
}

LevelMeter::~LevelMeter()
{
    stopTimer();
    detach();
}

void LevelMeter::attachTo (te::Plugin* levelMeterPlugin)
{
    detach();

    if (auto* lm = dynamic_cast<te::LevelMeterPlugin*> (levelMeterPlugin))
    {
        owner = levelMeterPlugin;
        measurer = &lm->measurer;
        measurer->addClient (client);
    }
}

void LevelMeter::detach()
{
    if (measurer != nullptr)
        measurer->removeClient (client);

    measurer = nullptr;
    owner = nullptr;
}

void LevelMeter::timerCallback()
{
    if (measurer == nullptr)
        return;

    const int used = juce::jmax (1, client.getNumChannelsUsed());

    for (int ch = 0; ch < 2; ++ch)
    {
        // Mono tracks mirror channel 1 onto channel 2
        const float db = ch < used ? client.getAndClearAudioLevel (ch).dB : levels[0];
        levels[ch] = juce::jmax (db, levels[ch] - 1.5f);
    }

    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1a));

    const bool horizontal = getWidth() > getHeight();
    auto area = getLocalBounds().reduced (1);

    juce::Rectangle<int> lanes[2];

    if (horizontal)
    {
        lanes[0] = area.removeFromTop (area.getHeight() / 2);
        lanes[1] = area;
    }
    else
    {
        lanes[0] = area.removeFromLeft (area.getWidth() / 2);
        lanes[1] = area;
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        const float db = levels[ch];
        const float norm = juce::jmap (juce::jlimit (-60.0f, 6.0f, db), -60.0f, 6.0f, 0.0f, 1.0f);

        const auto colour = db > 0.0f  ? juce::Colours::red
                          : db > -6.0f ? juce::Colours::yellow
                                       : juce::Colour (0xff43a047);

        auto lane = lanes[ch];
        auto fill = horizontal ? lane.removeFromLeft ((int) (lane.getWidth() * norm))
                               : lane.removeFromBottom ((int) (lane.getHeight() * norm));

        g.setColour (colour);
        g.fillRect (fill);
    }
}
