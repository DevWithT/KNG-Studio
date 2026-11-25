#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <map>
#include <vector>
#include "EditSession.h"

/** Every plugin on every lane and on the master, busiest first, with how much of the audio buffer it uses.
    View > Plugin CPU Monitor... opens it. Press Play to see readings. */
class PluginCpuMonitor : public juce::Component,
                         private juce::ListBoxModel,
                         private juce::Timer
{
public:
    explicit PluginCpuMonitor (EditSession& sessionToUse)
        : session (sessionToUse)
    {
        addAndMakeVisible (hint);
        hint.setFont (juce::Font (juce::FontOptions (12.0f)));
        hint.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
        hint.setText ("Press Play to see readings. Each figure is the share of the audio buffer that plugin uses.",
                      juce::dontSendNotification);

        addAndMakeVisible (totalLabel);
        totalLabel.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));

        addAndMakeVisible (list);
        list.setModel (this);
        list.setRowHeight (24);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);

        refresh();
        startTimerHz (5);
        setSize (560, 420);
    }

    ~PluginCpuMonitor() override
    {
        stopTimer();
        list.setModel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1e2124));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        hint.setBounds (r.removeFromTop (20));
        totalLabel.setBounds (r.removeFromTop (24));
        r.removeFromTop (4);
        list.setBounds (r);
    }

    /** Opens the window, or brings the open one to the front. Returns the new window, or nullptr if one was already open. */
    static juce::DialogWindow* open (EditSession& session)
    {
        static juce::Component::SafePointer<juce::DialogWindow> existing;

        if (existing != nullptr)
        {
            existing->setVisible (true);
            existing->toFront (true);
            return nullptr;
        }

        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (new PluginCpuMonitor (session));
        o.dialogTitle = "Plugin CPU Monitor";
        o.dialogBackgroundColour = juce::Colour (0xff1e2124);
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = true;

        auto* w = o.create();
        w->setVisible (true);
        existing = w;
        return w;
    }

private:
    struct Row
    {
        juce::String plugin, lane;
        float cpu = 0.0f;
    };

    void timerCallback() override
    {
        refresh();
    }

    void refresh()
    {
        std::map<tracktion::Plugin*, float> next;
        std::vector<Row> found;

        auto scan = [&] (tracktion::PluginList& plugins, const juce::String& lane)
        {
            for (auto* p : plugins)
            {
                if (p == nullptr || ! p->shouldMeasureCpuUsage())
                    continue;

                const float now = (float) p->getCpuUsage();
                const auto old = smoothed.find (p);
                const float value = old != smoothed.end() ? old->second + (now - old->second) * 0.3f : now;

                next[p] = value;
                found.push_back ({ p->getDisplayName(), lane, value });
            }
        };

        scan (session.getEdit().getMasterPluginList(), "Master");

        for (int i = 0; i < session.getNumTracks(); ++i)
            if (auto* t = session.getTrack (i))
                scan (t->pluginList, t->getName());

        smoothed = std::move (next);
        std::sort (found.begin(), found.end(), [] (const Row& a, const Row& b) { return a.cpu > b.cpu; });
        rows = std::move (found);

        float total = 0.0f;

        for (auto& r : rows)
            total += r.cpu;

        totalLabel.setText ("All plugins together: " + juce::String (juce::roundToInt (total * 100.0f)) + "% of the audio buffer",
                            juce::dontSendNotification);

        list.updateContent();
        list.repaint();
    }

    int getNumRows() override
    {
        return (int) rows.size();
    }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool) override
    {
        if (row < 0 || row >= (int) rows.size())
            return;

        const auto& r = rows[(size_t) row];

        if (row % 2 == 1)
        {
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.fillRect (0, 0, width, height);
        }

        g.setFont (juce::Font (juce::FontOptions (13.0f)));

        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawText (r.plugin, 8, 0, 200, height, juce::Justification::centredLeft, true);

        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawText (r.lane, 214, 0, 140, height, juce::Justification::centredLeft, true);

        const int barX = 362;
        const int barW = juce::jmax (20, width - barX - 70);
        const auto colour = r.cpu < 0.10f ? juce::Colour (0xff5ad45a)
                          : r.cpu < 0.30f ? juce::Colour (0xffe6c23a)
                                          : juce::Colour (0xffe6503a);

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (barX, height / 2 - 4, barW, 8);
        g.setColour (colour);
        g.fillRect (barX, height / 2 - 4, juce::roundToInt ((float) barW * juce::jmin (1.0f, r.cpu * 4.0f)), 8);

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawText (juce::String (r.cpu * 100.0f, 1) + "%", width - 62, 0, 56, height, juce::Justification::centredRight);
    }

    EditSession& session;
    juce::Label hint, totalLabel;
    juce::ListBox list;
    std::vector<Row> rows;
    std::map<tracktion::Plugin*, float> smoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginCpuMonitor)
};
