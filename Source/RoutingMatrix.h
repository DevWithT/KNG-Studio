#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"
#include "RouteMenu.h"

/** A grid of what feeds what. One row per lane, a column for where its Output goes and one column
    for each bus. A bus cell is a send level you drag. Greyed cells would make a loop. */
class RoutingMatrix : public juce::Component,
                      private juce::Timer
{
public:
    explicit RoutingMatrix (EditSession& sessionToUse)
        : session (sessionToUse)
    {
        addAndMakeVisible (hint);
        hint.setFont (juce::Font (juce::FontOptions (12.0f)));
        hint.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
        hint.setText ("Drag a cell to send that lane to a bus. Click Output to change where a lane goes. Greyed cells would make a loop.",
                      juce::dontSendNotification);

        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&grid, false);
        viewport.setScrollBarsShown (true, true);

        rebuild();
        startTimerHz (4);
        setSize (780, 440);
    }

    ~RoutingMatrix() override
    {
        stopTimer();
        viewport.setViewedComponent (nullptr, false);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1e2124));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        hint.setBounds (r.removeFromTop (20));
        r.removeFromTop (6);
        viewport.setBounds (r);
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
        o.content.setOwned (new RoutingMatrix (session));
        o.dialogTitle = "Routing Matrix";
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
    //==========================================================================
    struct SendCell : public juce::Slider
    {
        SendCell (int trackIndexToUse, int busToUse) : trackIndex (trackIndexToUse), bus (busToUse) {}

        const int trackIndex, bus;
    };

    struct Grid : public juce::Component
    {
        void paint (juce::Graphics& g) override
        {
            // A faint stripe on every other row, the header row excluded
            g.setColour (juce::Colours::white.withAlpha (0.04f));

            for (int r = 1; r < rows; r += 2)
                g.fillRect (0, r * rowHeight, getWidth(), rowHeight);
        }

        int rows = 0;
        int rowHeight = 28;
    };

    //==========================================================================
    void timerCallback() override
    {
        if (makeSignature() != signature)
        {
            rebuild();
            return;
        }

        // Follow changes made elsewhere, but never fight a cell being dragged
        for (auto* c : cells)
            if (! c->isMouseButtonDown())
                c->setValue ((double) session.getSendToBusDb (c->trackIndex, c->bus), juce::dontSendNotification);
    }

    juce::String makeSignature()
    {
        juce::String sig;
        sig << session.getNumTracks() << "|";

        for (int b : session.getBusNumbers())
            sig << b << session.getBusName (b) << ",";

        for (int i = 0; i < session.getNumTracks(); ++i)
        {
            if (auto* t = session.getTrack (i))
            {
                sig << "|" << t->itemID.toString() << t->getName() << ">" << KngRoute::currentName (*t) << ":";

                for (int b : session.getBusTargets (i))
                    sig << b << ",";
            }
        }

        return sig;
    }

    void rebuild()
    {
        cells.clear();
        outputButtons.clear();
        labels.clear();

        const int nameW = 150, outW = 170, cellW = 130, rowH = 28;
        const auto buses = session.getBusNumbers();

        auto addLabel = [this] (const juce::String& text, juce::Rectangle<int> bounds, bool bold)
        {
            auto* l = labels.add (new juce::Label ({}, text));
            l->setFont (juce::Font (juce::FontOptions (12.5f, bold ? juce::Font::bold : juce::Font::plain)));
            l->setMinimumHorizontalScale (0.8f);
            l->setBounds (bounds);
            grid.addAndMakeVisible (l);
        };

        // Header row
        int y = 0;
        addLabel ("Lane", { 4, y, nameW - 4, rowH }, true);
        addLabel ("Output", { nameW, y, outW, rowH }, true);

        for (int k = 0; k < buses.size(); ++k)
            addLabel (session.getBusName (buses[k]) + " send", { nameW + outW + k * cellW, y, cellW, rowH }, true);

        y += rowH;

        // One row per lane
        for (int i = 0; i < session.getNumTracks(); ++i)
        {
            auto* t = session.getTrack (i);

            if (t == nullptr)
                continue;

            addLabel (t->getName(), { 4, y, nameW - 8, rowH }, false);

            auto* ob = outputButtons.add (new juce::TextButton());
            ob->setButtonText (KngRoute::currentName (*t));
            ob->setTooltip ("Where this lane's sound goes. Click to change it.");
            ob->setBounds (juce::Rectangle<int> (nameW, y, outW, rowH).reduced (4, 2));
            ob->onClick = [this, ob, i]
            {
                if (auto* track = session.getTrack (i))
                {
                    juce::PopupMenu menu;
                    KngRoute::addItems (menu, session, *track);
                    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (ob));
                }
            };
            grid.addAndMakeVisible (ob);

            const auto targets = session.getBusTargets (i);

            for (int k = 0; k < buses.size(); ++k)
            {
                const int bus = buses[k];
                auto* cell = cells.add (new SendCell (i, bus));

                cell->setSliderStyle (juce::Slider::LinearHorizontal);
                cell->setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
                cell->setRange (-60.0, 6.0, 0.1);
                cell->setDoubleClickReturnValue (true, -60.0);
                cell->textFromValueFunction = [] (double v)
                {
                    return v <= -60.0 ? juce::String ("Off") : juce::String (v, 1) + " dB";
                };
                cell->setValue ((double) session.getSendToBusDb (i, bus), juce::dontSendNotification);
                cell->setEnabled (targets.contains (bus));
                cell->setTooltip ("Send level from this lane to " + session.getBusName (bus) + ". Double-click for Off.");

                cell->onDragStart   = [this] { session.beginEdit(); };
                cell->onValueChange = [this, cell] { session.setSendToBusDb (cell->trackIndex, cell->bus, (float) cell->getValue()); };
                cell->onDragEnd     = [this] { session.save(); };

                cell->setBounds (juce::Rectangle<int> (nameW + outW + k * cellW, y, cellW, rowH).reduced (4, 2));
                grid.addAndMakeVisible (cell);
            }

            y += rowH;
        }

        grid.rows = y / rowH;
        grid.rowHeight = rowH;
        grid.setSize (nameW + outW + juce::jmax (1, buses.size()) * cellW + 8, y);
        grid.repaint();

        signature = makeSignature();
    }

    EditSession& session;
    juce::String signature;

    juce::Label hint;
    juce::Viewport viewport;
    Grid grid;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::TextButton> outputButtons;
    juce::OwnedArray<SendCell> cells;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoutingMatrix)
};
