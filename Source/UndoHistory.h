#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"
#include "KngTheme.h"

// Edit > Undo History: the steps you can redo (furthest first), then "Now", then the steps you can undo (newest first).
// Click a step to go back or forward to it. A step without a name shows as "Edit N".
class UndoHistoryList : public juce::Component,
                        private juce::ListBoxModel,
                        private juce::Timer
{
public:
    explicit UndoHistoryList (EditSession& sessionToUse)
        : session (sessionToUse)
    {
        addAndMakeVisible (list);
        list.setModel (this);
        list.setRowHeight (26);
        list.setColour (juce::ListBox::backgroundColourId, KngTheme::bg);
        setSize (340, 440);
        refill();
        startTimerHz (4);
    }

    ~UndoHistoryList() override
    {
        stopTimer();
        list.setModel (nullptr);
    }

    void resized() override { list.setBounds (getLocalBounds()); }

    // Opens the window, one at a time. Returns the new window, or nullptr if one was already open (it is brought forward).
    static juce::DialogWindow* open (EditSession& session)
    {
        if (openWindow() != nullptr)
        {
            openWindow()->toFront (true);
            return nullptr;
        }

        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned (new UndoHistoryList (session));
        options.dialogTitle = "Undo History";
        options.dialogBackgroundColour = KngTheme::bg;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = false;
        options.resizable = true;

        auto* window = options.launchAsync();
        openWindow() = window;
        return window;
    }

private:
    static juce::Component::SafePointer<juce::DialogWindow>& openWindow()
    {
        static juce::Component::SafePointer<juce::DialogWindow> window;
        return window;
    }

    static juce::String labelFor (const juce::String& name, int number)
    {
        return name.isNotEmpty() ? name : "Edit " + juce::String (number);
    }

    void refill()
    {
        auto& um = session.getEdit().getUndoManager();
        redoNames = um.getRedoDescriptions();
        undoNames = um.getUndoDescriptions();
        list.updateContent();
        list.scrollToEnsureRowIsOnscreen (redoNames.size());
        list.repaint();
    }

    void timerCallback() override
    {
        auto& um = session.getEdit().getUndoManager();

        if (um.getUndoDescriptions() != undoNames || um.getRedoDescriptions() != redoNames)
            refill();
    }

    int getNumRows() override
    {
        return redoNames.size() + 1 + undoNames.size();
    }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool) override
    {
        const int redoCount = redoNames.size();
        juce::String text;
        juce::Colour colour = KngTheme::text;

        if (row < redoCount)
        {
            const int k = redoCount - 1 - row;

            if (! juce::isPositiveAndBelow (k, redoNames.size()))
                return;

            text = labelFor (redoNames[k], undoNames.size() + 1 + k);
            colour = KngTheme::muted;
        }
        else if (row == redoCount)
        {
            text = "Now";
            g.fillAll (KngTheme::accentSoft);
        }
        else
        {
            const int u = row - redoCount - 1;

            if (! juce::isPositiveAndBelow (u, undoNames.size()))
                return;

            text = labelFor (undoNames[u], undoNames.size() - u);
        }

        g.setColour (KngTheme::line);
        g.drawHorizontalLine (height - 1, 0.0f, (float) width);
        g.setColour (colour);
        g.setFont (14.0f);
        g.drawText (text, 10, 0, width - 20, height, juce::Justification::centredLeft);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        const int redoCount = redoNames.size();

        if (row < redoCount)
        {
            for (int i = 0; i < redoCount - row; ++i)
                session.redo();
        }
        else if (row > redoCount)
        {
            for (int i = 0; i < row - redoCount; ++i)
                session.undo();
        }

        refill();
    }

    EditSession& session;
    juce::ListBox list { "history" };
    juce::StringArray undoNames, redoNames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoHistoryList)
};
