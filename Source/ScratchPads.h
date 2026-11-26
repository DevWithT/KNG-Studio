#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "EditSession.h"
#include "KngTheme.h"

// Scratch Pads: text pads saved with the song (lyrics, ideas, to-dos). Saved in the song's "kngPads" property.
class ScratchPads : public juce::Component, private juce::Timer
{
public:
    explicit ScratchPads (EditSession& s) : session (s)
    {
        load();

        addAndMakeVisible (padBox);
        addAndMakeVisible (newButton);
        addAndMakeVisible (renameButton);
        addAndMakeVisible (deleteButton);
        addAndMakeVisible (editor);

        padBox.setTooltip ("Pick a pad");
        newButton.setTooltip ("Start a new pad");
        renameButton.setTooltip ("Rename this pad");
        deleteButton.setTooltip ("Delete this pad");

        editor.setMultiLine (true, true);
        editor.setReturnKeyStartsNewLine (true);
        editor.setFont (juce::Font (juce::FontOptions (16.0f)));
        editor.setColour (juce::TextEditor::backgroundColourId, KngTheme::panel);
        editor.setColour (juce::TextEditor::textColourId, KngTheme::text);
        editor.setColour (juce::TextEditor::outlineColourId, KngTheme::btn);
        editor.setTextToShowWhenEmpty ("Lyrics, ideas, to-dos...", KngTheme::muted);
        editor.onTextChange = [this]
        {
            commit();
            startTimer (1500);
        };

        padBox.onChange = [this] { switchTo (padBox.getSelectedId() - 1); };

        newButton.onClick = [this]
        {
            askName ("New pad", {}, [this] (const juce::String& name)
            {
                commit();
                pads.push_back ({ name, {} });
                refreshBox ((int) pads.size() - 1);
                persist();
            });
        };

        renameButton.onClick = [this]
        {
            askName ("Rename pad", pads[(size_t) current].name, [this] (const juce::String& name)
            {
                commit();
                pads[(size_t) current].name = name;
                refreshBox (current);
                persist();
            });
        };

        deleteButton.onClick = [this]
        {
            juce::Component::SafePointer<ScratchPads> safe (this);

            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Delete pad",
                                                "Delete \"" + pads[(size_t) current].name + "\"? This can't be undone.",
                                                "Delete", "Cancel", this,
                                                juce::ModalCallbackFunction::create ([safe] (int result)
                                                {
                                                    if (safe != nullptr && result == 1)
                                                        safe->deletePad();
                                                }));
        };

        refreshBox (0);
        setSize (520, 420);
    }

    ~ScratchPads() override
    {
        if (isTimerRunning())
            persist();
    }

    void paint (juce::Graphics& g) override   { g.fillAll (KngTheme::bg); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto top = r.removeFromTop (30);
        deleteButton.setBounds (top.removeFromRight (70));
        top.removeFromRight (6);
        renameButton.setBounds (top.removeFromRight (70));
        top.removeFromRight (6);
        newButton.setBounds (top.removeFromRight (70));
        top.removeFromRight (8);
        padBox.setBounds (top);

        r.removeFromTop (8);
        editor.setBounds (r);
    }

    /** Opens the window (or brings it forward). Returns the new window, or nullptr if it was already open. */
    static juce::DialogWindow* open (EditSession& s)
    {
        static juce::Component::SafePointer<juce::DialogWindow> existing;

        if (existing != nullptr)
        {
            existing->setVisible (true);
            existing->toFront (true);
            return nullptr;
        }

        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (new ScratchPads (s));
        o.dialogTitle = "Scratch Pads";
        o.dialogBackgroundColour = KngTheme::bg;
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = false;
        o.resizable = true;

        auto* w = o.create();
        w->setVisible (true);
        existing = w;
        return w;
    }

private:
    struct Pad { juce::String name, text; };

    void load()
    {
        const auto parsed = juce::JSON::parse (session.getEdit().state.getProperty ("kngPads").toString());

        if (auto* list = parsed.getArray())
            for (auto& v : *list)
                pads.push_back ({ v["name"].toString(), v["text"].toString() });

        if (pads.empty())
            pads.push_back ({ "Notes", {} });
    }

    void commit()
    {
        if (current >= 0 && current < (int) pads.size())
            pads[(size_t) current].text = editor.getText();
    }

    void persist()
    {
        stopTimer();
        commit();

        juce::Array<juce::var> list;

        for (auto& p : pads)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("name", p.name);
            o->setProperty ("text", p.text);
            list.add (juce::var (o));
        }

        session.getEdit().state.setProperty ("kngPads", juce::JSON::toString (juce::var (list), true), nullptr);
        session.save();
    }

    void timerCallback() override   { persist(); }

    void refreshBox (int select)
    {
        current = juce::jlimit (0, (int) pads.size() - 1, select);
        padBox.clear (juce::dontSendNotification);

        for (int i = 0; i < (int) pads.size(); ++i)
            padBox.addItem (pads[(size_t) i].name, i + 1);

        padBox.setSelectedId (current + 1, juce::dontSendNotification);
        editor.setText (pads[(size_t) current].text, false);
    }

    void switchTo (int index)
    {
        commit();
        current = juce::jlimit (0, (int) pads.size() - 1, index);
        editor.setText (pads[(size_t) current].text, false);
    }

    void deletePad()
    {
        if (pads.size() <= 1)
        {
            pads[0].text = {};
            refreshBox (0);
        }
        else
        {
            pads.erase (pads.begin() + current);
            refreshBox (juce::jmax (0, current - 1));
        }

        persist();
    }

    void askName (const juce::String& title, const juce::String& initial,
                  std::function<void (const juce::String&)> done)
    {
        auto* w = new juce::AlertWindow (title, "Name this pad:", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", initial);
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        juce::Component::SafePointer<ScratchPads> safe (this);

        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, done] (int result)
        {
            if (safe == nullptr || result != 1)
                return;

            const auto name = w->getTextEditorContents ("name").trim();

            if (name.isNotEmpty())
                done (name);
        }), true);
    }

    EditSession& session;
    std::vector<Pad> pads;
    int current = 0;

    juce::ComboBox padBox;
    juce::TextButton newButton { "New" }, renameButton { "Rename" }, deleteButton { "Delete" };
    juce::TextEditor editor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScratchPads)
};
